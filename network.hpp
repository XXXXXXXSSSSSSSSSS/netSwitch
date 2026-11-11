#pragma once
#include "common.hpp"
struct ComScope{ComScope(){CoInitializeEx(NULL,COINIT_MULTITHREADED);}~ComScope(){CoUninitialize();}};
struct Wmi {
 IWbemLocator* locator;IWbemServices* services;
 Wmi():locator(NULL),services(NULL){
  HRESULT h=CoCreateInstance(CLSID_WbemLocator,NULL,CLSCTX_INPROC_SERVER,IID_IWbemLocator,(void**)&locator);
  if(FAILED(h))throw std::runtime_error("WMI locator");
  BSTR ns=SysAllocString(L"ROOT\\CIMV2");h=locator->ConnectServer(ns,NULL,NULL,NULL,0,NULL,NULL,&services);SysFreeString(ns);
  if(FAILED(h)){locator->Release();throw std::runtime_error("WMI connect");}
  h=CoSetProxyBlanket(services,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,NULL,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE);
  if(FAILED(h)){services->Release();locator->Release();throw std::runtime_error("WMI security");}
 }
 ~Wmi(){services->Release();locator->Release();}
 static std::wstring str(IWbemClassObject* o,const wchar_t* n){VARIANT v;VariantInit(&v);o->Get(n,0,&v,NULL,NULL);std::wstring s=(v.vt==VT_BSTR && v.bstrVal)?v.bstrVal:L"";VariantClear(&v);return s;}
 static long num(IWbemClassObject* o,const wchar_t* n,long fallback=0){VARIANT v;VariantInit(&v);o->Get(n,0,&v,NULL,NULL);long x=fallback;if(v.vt==VT_I4||v.vt==VT_UI4)x=v.lVal;else if(v.vt==VT_BOOL)x=v.boolVal!=VARIANT_FALSE;VariantClear(&v);return x;}
 std::vector<Adapter> list(){
  BSTR lang=SysAllocString(L"WQL"),query=SysAllocString(L"SELECT GUID,NetConnectionID,Name,NetEnabled,NetConnectionStatus,AdapterTypeID,PNPDeviceID,ConfigManagerErrorCode FROM Win32_NetworkAdapter WHERE PhysicalAdapter=TRUE");
  IEnumWbemClassObject* en=NULL;HRESULT hr=services->ExecQuery(lang,query,WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,NULL,&en);SysFreeString(lang);SysFreeString(query);if(FAILED(hr))throw std::runtime_error("WMI query");
  std::vector<Adapter> out;IWbemClassObject* o=NULL;ULONG got;
  while(true){hr=en->Next(4000,1,&o,&got);if(FAILED(hr)||hr==WBEM_S_TIMEDOUT){en->Release();throw std::runtime_error("WMI timeout");}if(!got)break;
   std::wstring g=str(o,L"GUID"),n=str(o,L"NetConnectionID"),pnp=str(o,L"PNPDeviceID");
   if(guidValid(g.c_str()) && !n.empty() && (pnp.find(L"PCI\\")==0||pnp.find(L"USB\\")==0||pnp.find(L"PCMCIA\\")==0)){
    Adapter a={};wcsncpy(a.guid,g.c_str(),39);wcsncpy(a.name,n.c_str(),259);a.enabled=num(o,L"ConfigManagerErrorCode",-1)==22?0:1;a.connected=num(o,L"NetConnectionStatus")==2;
    GUID id;NET_LUID luid={};MIB_IF_ROW2 row={};CLSIDFromString(a.guid,&id);
    if(ConvertInterfaceGuidToLuid(&id,&luid)==NO_ERROR){row.InterfaceLuid=luid;if(GetIfEntry2(&row)==NO_ERROR){if(row.Type==71)a.kind=1;else if(row.Type==6)a.kind=2;}}
    if(!a.kind && num(o,L"AdapterTypeID",-1)==9)a.kind=1;
    if(!a.kind)a.kind=3; // Unknown/offline device: user must explicitly choose its role.
    if(a.kind)out.push_back(a);
   }o->Release();
  }en->Release();return out;
 }
 bool enabled(const std::wstring& guid,bool& value){auto all=list();for(auto& a:all)if(_wcsicmp(a.guid,guid.c_str())==0){value=a.enabled!=0;return true;}return false;}
 bool setEnabled(const std::wstring& guid,bool value){
  if(!guidValid(guid.c_str()))return false;
  // Resolve the method path from inventory, never from client-provided WMI text.
  BSTR lang=SysAllocString(L"WQL"),query=SysAllocString(L"SELECT __PATH,GUID FROM Win32_NetworkAdapter WHERE PhysicalAdapter=TRUE");IEnumWbemClassObject* en=NULL;HRESULT hr=services->ExecQuery(lang,query,WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,NULL,&en);SysFreeString(lang);SysFreeString(query);if(FAILED(hr))return false;
  std::wstring path;IWbemClassObject* o;ULONG got;
  while(SUCCEEDED(hr=en->Next(3000,1,&o,&got))&&got){if(_wcsicmp(str(o,L"GUID").c_str(),guid.c_str())==0)path=str(o,L"__PATH");o->Release();if(!path.empty())break;}en->Release();if(path.empty())return false;
  BSTR p=SysAllocString(path.c_str()),m=SysAllocString(value?L"Enable":L"Disable");IWbemClassObject* result=NULL;hr=services->ExecMethod(p,m,0,NULL,NULL,&result,NULL);SysFreeString(p);SysFreeString(m);bool ok=SUCCEEDED(hr)&&result&&num(result,L"ReturnValue",-1)==0;if(result)result->Release();return ok;
 }
};
struct Value{DWORD automatic,metric;};
inline bool equal(Value a,Value b){return a.automatic==b.automatic && (a.automatic || a.metric==b.metric);}
struct Entry{wchar_t guid[40];DWORD family,attempted;Value before,after;};
struct Snapshot{DWORD magic,count;Entry entries[12];DWORD checksum;};
inline DWORD crc(const Snapshot& j){DWORD h=2166136261u;auto p=(const BYTE*)&j;for(size_t i=0;i<offsetof(Snapshot,checksum);++i)h=(h^p[i])*16777619u;return h;}
struct Network {
 Wmi wmi;
 bool ready(const wchar_t* guid){GUID g;NET_LUID l={};if(FAILED(CLSIDFromString((LPWSTR)guid,&g))||ConvertInterfaceGuidToLuid(&g,&l)!=NO_ERROR)return false;PMIB_UNICASTIPADDRESS_TABLE t=NULL;if(GetUnicastIpAddressTable(AF_UNSPEC,&t)!=NO_ERROR)return false;bool found=false;for(ULONG i=0;i<t->NumEntries;++i)if(t->Table[i].InterfaceLuid.Value==l.Value&&t->Table[i].DadState==IpDadStatePreferred)found=true;FreeMibTable(t);return found;}
 bool read(const Entry& e,Value& v){if(e.family==0){bool b;if(!wmi.enabled(e.guid,b))return false;v={0,b?1u:0u};return true;}GUID g;NET_LUID l={};if(!guidValid(e.guid)||FAILED(CLSIDFromString((LPWSTR)e.guid,&g))||ConvertInterfaceGuidToLuid(&g,&l)!=NO_ERROR)return false;MIB_IPINTERFACE_ROW row={};row.InterfaceLuid=l;row.Family=(ADDRESS_FAMILY)e.family;if(GetIpInterfaceEntry(&row)!=NO_ERROR)return false;v={(DWORD)(row.UseAutomaticMetric!=0),row.Metric};return true;}
 bool write(const Entry& e,Value v){if(e.family==0)return wmi.setEnabled(e.guid,v.metric!=0);GUID g;NET_LUID l={};if(FAILED(CLSIDFromString((LPWSTR)e.guid,&g))||ConvertInterfaceGuidToLuid(&g,&l)!=NO_ERROR)return false;MIB_IPINTERFACE_ROW row={};row.InterfaceLuid=l;row.Family=(ADDRESS_FAMILY)e.family;if(GetIpInterfaceEntry(&row)!=NO_ERROR)return false;row.UseAutomaticMetric=(BOOLEAN)v.automatic;row.Metric=v.metric;if(e.family==AF_INET)row.SitePrefixLength=0;return SetIpInterfaceEntry(&row)==NO_ERROR;}
};
struct Store {
 std::wstring file;Store(const std::wstring& f):file(f){}
 bool save(Snapshot j){j.magic=MAGIC;j.checksum=crc(j);std::wstring tmp=file+L".tmp";HANDLE h=CreateFileW(tmp.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);if(h==INVALID_HANDLE_VALUE)return false;DWORD n=0;bool ok=WriteFile(h,&j,sizeof(j),&n,NULL)&&n==sizeof(j)&&FlushFileBuffers(h);CloseHandle(h);if(!ok)return false;return MoveFileExW(tmp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}
 int load(Snapshot& j){HANDLE h=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND?0:-1;DWORD n=0;bool ok=GetFileSize(h,NULL)==sizeof(j)&&ReadFile(h,&j,sizeof(j),&n,NULL)&&n==sizeof(j);CloseHandle(h);if(!ok||j.magic!=MAGIC||j.count>12||crc(j)!=j.checksum)return -1;for(DWORD i=0;i<j.count;++i){auto& e=j.entries[i];e.guid[39]=0;if(!guidValid(e.guid)||(e.family!=0&&e.family!=AF_INET&&e.family!=AF_INET6)||e.before.automatic>1||e.after.automatic>1||e.attempted>1)return -1;}return 1;}
 bool clear(){return DeleteFileW(file.c_str())||GetLastError()==ERROR_FILE_NOT_FOUND;}
};
template<class N> bool recover(N& n,Store& store){Snapshot j={};int exists=store.load(j);if(exists==0)return true;if(exists<0)return false;bool ok=true;for(DWORD i=j.count;i>0;--i){auto& e=j.entries[i-1];if(!e.attempted)continue;Value now={};if(!n.read(e,now)){ok=false;continue;}if(equal(now,e.before))continue;if(!equal(now,e.after)){ok=false;continue;}if(!n.write(e,e.before)){ok=false;continue;}if(!n.read(e,now)||!equal(now,e.before))ok=false;}return ok&&store.clear();}
template<class N> bool commit(N& n,Store& store,Snapshot& j){Snapshot old={};if(store.load(old)!=0||!j.count||!store.save(j))return false;for(DWORD i=0;i<j.count;++i){auto& e=j.entries[i];Value now={};if(!n.read(e,now)||!equal(now,e.before)){recover(n,store);return false;}e.attempted=1;if(!store.save(j)){recover(n,store);return false;}if(!equal(e.before,e.after)&&!n.write(e,e.after)){recover(n,store);return false;}bool matched=false;for(int k=0;k<12;++k){if(n.read(e,now)&&equal(now,e.after)){matched=true;break;}Sleep(250);}if(!matched){recover(n,store);return false;}}return true;}
