#define wWinMain serviceProgramEntry
#include "service.cpp"
#undef wWinMain
#include <map>
#include <cassert>
#include <iostream>
std::wstring wg=L"{11111111-1111-1111-1111-111111111111}",eg=L"{22222222-2222-2222-2222-222222222222}";
struct FakeWmi{std::vector<Adapter> devices;std::vector<Adapter> list(){return devices;}};
struct Fake {
 FakeWmi wmi;std::map<std::pair<std::wstring,DWORD>,Value> values;int writes=0,failAt=-1;bool ipReady=true, alterOnFailure=false;
 Fake(){Adapter a={},b={};wcsncpy(a.guid,wg.c_str(),39);wcsncpy(b.guid,eg.c_str(),39);wcscpy(a.name,L"Wi-Fi");wcscpy(b.name,L"Ethernet");a.kind=1;b.kind=2;a.enabled=b.enabled=a.connected=b.connected=1;wmi.devices={a,b};for(auto g:{wg,eg}){values[{g,0}]={0,1};values[{g,AF_INET}]={1,25};values[{g,AF_INET6}]={1,25};}}
 bool read(const Entry& e,Value& v){auto i=values.find({e.guid,e.family});if(i==values.end())return false;v=i->second;return true;}
 bool write(const Entry& e,Value v){if(++writes==failAt){if(alterOnFailure)values[{wg,AF_INET6}]={0,5};return false;}values[{e.guid,e.family}]=v;if(!e.family)for(auto& d:wmi.devices)if(_wcsicmp(d.guid,e.guid)==0)d.enabled=v.metric;return true;}
 bool ready(const wchar_t*){return ipReady;}
};
Request q(DWORD command){Request r={};r.magic=MAGIC;r.command=command;wcscpy(r.wifi,wg.c_str());wcscpy(r.ethernet,eg.c_str());return r;}
Reply call(Fake& n,Store& s,DWORD c){return execute(n,s,q(c),GetCurrentProcessId());}
void reset(Store& s){releaseController();DeleteFileW(s.file.c_str());DeleteFileW((s.file+L".tmp").c_str());}
int main(){stopping=CreateEventW(NULL,TRUE,FALSE,NULL);std::wstring root=parent(exePath());Store store(pathJoin(root,L"acceptance-test.bin"));int tests=0;
 auto passed=[&](){++tests;reset(store);};reset(store);
 {Fake f;assert(call(f,store,PREFER_WIFI).code==0);assert(f.values[std::make_pair(wg,(DWORD)AF_INET)].metric==5);assert(f.values[std::make_pair(eg,(DWORD)AF_INET6)].metric==5000);assert(call(f,store,RESTORE).code==0);assert(f.values[std::make_pair(wg,(DWORD)AF_INET)].automatic==1);passed();}
 {Fake f;f.failAt=2;assert(call(f,store,PREFER_WIFI).code!=0);assert(f.values[std::make_pair(wg,(DWORD)AF_INET)].automatic==1);Snapshot j={};assert(store.load(j)==0);passed();}
 {Fake f;assert(call(f,store,PREFER_WIFI).code==0);f.values[{wg,AF_INET}]={0,77};assert(call(f,store,RESTORE).code!=0);assert(f.values[std::make_pair(wg,(DWORD)AF_INET)].metric==77);Snapshot j={};assert(store.load(j)==1);passed();}
 {Fake f;assert(call(f,store,PREFER_WIFI).code==0);releaseController();assert(recover(f,store));assert(f.values[std::make_pair(eg,(DWORD)AF_INET)].automatic==1);passed();}
 {Fake f;f.values.erase({eg,AF_INET6});assert(call(f,store,PREFER_WIFI).code!=0);assert(f.writes==0);passed();}
 {Fake f;f.values.erase({wg,AF_INET6});f.values.erase({eg,AF_INET6});assert(call(f,store,PREFER_WIFI).code==0);assert(call(f,store,RESTORE).code==0);passed();}
 {Fake f;f.wmi.devices[0].enabled=0;f.values[{wg,0}]={0,0};assert(call(f,store,PREFER_WIFI).code!=0);assert(f.writes==0);assert(call(f,store,ENABLE_WIFI).code==0);assert(f.values[std::make_pair(wg,0ul)].metric==1);Snapshot j={};assert(store.load(j)==0);passed();}
 {Fake f;assert(call(f,store,EXCLUSIVE_WIFI).code==0);assert(f.values[std::make_pair(eg,0ul)].metric==0);assert(call(f,store,EXCLUSIVE_ETH).code==0);assert(f.values[std::make_pair(wg,0ul)].metric==0);assert(f.values[std::make_pair(eg,0ul)].metric==1);assert(call(f,store,RESTORE).code==0);assert(f.values[std::make_pair(wg,0ul)].metric==1);passed();}
 {Fake f;f.wmi.devices[0].enabled=0;f.values[{wg,0}]={0,0};assert(call(f,store,EXCLUSIVE_WIFI).code==0);assert(f.values[std::make_pair(wg,0ul)].metric==1);assert(call(f,store,RESTORE).code==0);assert(f.values[std::make_pair(wg,0ul)].metric==0);assert(f.values[std::make_pair(eg,0ul)].metric==1);passed();}
 {Fake f;Request r=q(PREFER_WIFI);wcscpy(r.ethernet,r.wifi);assert(execute(f,store,r,GetCurrentProcessId()).code!=0);assert(f.writes==0);passed();}
 {Fake f;assert(call(f,store,LIST).count==2);assert(f.writes==0);passed();}
 {Fake f;assert(call(f,store,STATUS).code==0);assert(f.writes==0);passed();}
 {Fake f;Store bad(pathJoin(root,L"nonexistent-directory\\journal.bin"));assert(call(f,bad,PREFER_WIFI).code!=0);assert(f.writes==0);passed();}
 {Fake f;HANDLE h=CreateFileW(store.file.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);DWORD n;WriteFile(h,"bad",3,&n,NULL);CloseHandle(h);assert(call(f,store,PREFER_WIFI).code!=0);assert(f.writes==0);passed();}
 {Fake f;assert(call(f,store,PREFER_WIFI).code==0);Snapshot j={};assert(store.load(j)==1);j.entries[0].after.metric^=1;HANDLE h=CreateFileW(store.file.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);DWORD n;WriteFile(h,&j,sizeof(j),&n,NULL);CloseHandle(h);assert(call(f,store,RESTORE).code!=0);passed();}
 {Fake f;assert(call(f,store,PREFER_WIFI).code==0);f.values.erase({wg,AF_INET});assert(call(f,store,RESTORE).code!=0);Snapshot j={};assert(store.load(j)==1);f.values[{wg,AF_INET}]={0,5};assert(call(f,store,RESTORE).code==0);passed();}
 {Fake f;for(int i=0;i<10;++i){assert(call(f,store,PREFER_WIFI).code==0);assert(call(f,store,PREFER_ETH).code==0);}assert(call(f,store,RESTORE).code==0);passed();}
 {Fake f;f.wmi.devices[0].connected=0;assert(call(f,store,PREFER_WIFI).code!=0);assert(f.writes==0);passed();}
 {Fake f;f.ipReady=false;SetEvent(stopping);assert(call(f,store,EXCLUSIVE_WIFI).code!=0);assert(f.values[std::make_pair(eg,0ul)].metric==1);ResetEvent(stopping);passed();}
 {Fake f;assert(call(f,store,999).code!=0);assert(f.writes==0);passed();}
 {Fake f;assert(call(f,store,PREFER_WIFI).code==0);f.failAt=f.writes+1;assert(call(f,store,RESTORE).code!=0);f.failAt=-1;assert(call(f,store,RESTORE).code==0);passed();}
 {Fake f;Request invalid=q(PREFER_WIFI);wcscpy(invalid.wifi,L"not-a-guid");assert(execute(f,store,invalid,GetCurrentProcessId()).code!=0);assert(f.writes==0);passed();}
 {Fake f;f.failAt=1;f.alterOnFailure=true;assert(call(f,store,PREFER_WIFI).code!=0);assert(f.values[std::make_pair(wg,(DWORD)AF_INET6)].automatic==0);assert(f.values[std::make_pair(wg,(DWORD)AF_INET6)].metric==5);passed();}
 CloseHandle(stopping);std::cout<<tests<<" production-code acceptance scenarios passed (simulated adapters; real journal IO).\n";return 0;}
