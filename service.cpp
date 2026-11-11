#include "network.hpp"
SERVICE_STATUS_HANDLE serviceHandle=NULL;SERVICE_STATUS serviceStatus={};HANDLE stopping=NULL,controller=NULL;DWORD controllerPid=0;
void report(DWORD state,DWORD error=0){serviceStatus.dwServiceType=SERVICE_WIN32_OWN_PROCESS;serviceStatus.dwCurrentState=state;serviceStatus.dwControlsAccepted=state==SERVICE_RUNNING?SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN:0;serviceStatus.dwWin32ExitCode=error;serviceStatus.dwWaitHint=state==SERVICE_STOP_PENDING?30000:0;SetServiceStatus(serviceHandle,&serviceStatus);}
void WINAPI control(DWORD c){if(c==SERVICE_CONTROL_STOP||c==SERVICE_CONTROL_SHUTDOWN){report(SERVICE_STOP_PENDING);SetEvent(stopping);}}
void releaseController(){if(controller)CloseHandle(controller);controller=NULL;controllerPid=0;}
inline bool selectedPair(const Request& q,const std::vector<Adapter>& all,Adapter& wifi,Adapter& eth){bool w=false,e=false;for(auto& a:all){if(_wcsicmp(a.guid,q.wifi)==0&&(a.kind==1||a.kind==3)){wifi=a;w=true;}if(_wcsicmp(a.guid,q.ethernet)==0&&(a.kind==2||a.kind==3)){eth=a;e=true;}}return w&&e&&_wcsicmp(q.wifi,q.ethernet)!=0;}
template<class N> Reply execute(N& net,Store& store,const Request& q,DWORD pid){
 if(q.command==LIST){Reply r=response(0,L"已读取网卡状态");auto a=net.wmi.list();r.count=(DWORD)std::min<size_t>(a.size(),32);for(DWORD i=0;i<r.count;++i)r.adapters[i]=a[i];return r;}
 if(q.command==STATUS){Snapshot j={};int x=store.load(j);return response(x<0?1:0,x<0?L"恢复记录损坏，已停止网络操作":x?L"有待恢复的设置":L"当前没有由本程序保持的网络修改");}
 if(q.command< PREFER_WIFI||q.command>ENABLE_ETH)return response(1,L"不支持的操作");
 if(controller&&controllerPid!=pid)return response(1,L"另一登录会话正在控制网络，请先在该会话恢复并退出");
 HANDLE process=controller?controller:OpenProcess(SYNCHRONIZE,FALSE,pid);if(!process)return response(1,L"无法验证托盘进程");
 if(!controller){controller=process;controllerPid=pid;}
 if(q.command==RESTORE){bool ok=recover(net,store);if(ok)releaseController();return response(ok?0:1,ok?L"原设置已恢复":L"部分设置无法恢复：设备可能已移除或被其他程序修改。记录已保留，可稍后重试。");}
 if(!guidValid(q.wifi)||!guidValid(q.ethernet))return response(1,L"请先在设置里选择 Wi-Fi 和有线网卡");
 Adapter wifi={},eth={};auto all=net.wmi.list();if(!selectedPair(q,all,wifi,eth))return response(1,L"选定的网卡不存在或类型不匹配，请重新选择");
 // A new operation begins only after the previous transaction is restored.
 if(!recover(net,store))return response(1,L"上次修改尚未恢复；请连接原设备并点“恢复原设置”");
 all=net.wmi.list();if(!selectedPair(q,all,wifi,eth))return response(1,L"恢复期间设备状态已变化，请刷新网卡列表后重试");
 bool preferWifi=q.command==PREFER_WIFI||q.command==EXCLUSIVE_WIFI||q.command==ENABLE_WIFI;
 Adapter target=preferWifi?wifi:eth,other=preferWifi?eth:wifi;
 bool enableOnly=q.command==ENABLE_WIFI||q.command==ENABLE_ETH;
 bool exclusive=q.command==EXCLUSIVE_WIFI||q.command==EXCLUSIVE_ETH;
 Snapshot j={};j.magic=MAGIC;
 auto add=[&](const wchar_t* guid,DWORD family,Value after)->bool{if(j.count>=12)return false;Entry e={};wcsncpy(e.guid,guid,39);e.family=family;if(!net.read(e,e.before))return false;e.after=after;if(!equal(e.before,e.after))j.entries[j.count++]=e;return true;};
 if(enableOnly){
  if(!add(target.guid,0,Value{0,1}))return response(1,L"无法读取网卡状态");
  if(!j.count)return response(0,L"所选网卡已经启用");
  if(!commit(net,store,j))return response(1,L"启用失败，已尝试回滚，请检查设备状态");
  // Explicit user repair is retained, unlike a temporary switching transaction.
  if(!store.clear())return response(1,L"网卡已启用，但恢复记录未能清理，请点恢复原设置");
  releaseController();return response(0,L"已重新启用所选网卡；网络连接可能需要几秒钟");
 }
 if(!exclusive){
  if(!target.enabled||!other.enabled)return response(1,L"优先模式需要两张网卡均启用。可在设置里点“重新启用”修复旧版禁用的网卡。");
  if(!target.connected)return response(1,L"目标网络尚未连接。请先插入网线或在 Windows 中连接 Wi-Fi。");
  int families[]={AF_INET,AF_INET6};unsigned usable=0;
  for(int family:families){Entry a={},b={};wcsncpy(a.guid,target.guid,39);wcsncpy(b.guid,other.guid,39);a.family=b.family=family;Value av={},bv={};bool ar=net.read(a,av),br=net.read(b,bv);
   if(!ar&&!br)continue;
   if(ar!=br)return response(1,L"两张网卡的 IPv4/IPv6 配置不一致，优先模式无法保证一致切换，请先检查网络设置");
   if(!add(target.guid,family,Value{0,5})||!add(other.guid,family,Value{0,5000}))return response(1,L"读取网络参数失败，尚未修改设置");++usable;
  }if(!usable)return response(1,L"没有可用的 IPv4/IPv6 接口");
 }else{
  // Enable the target in its own recoverable phase before considering disabling
  // the peer. Never disable the peer if target link/IP has not become ready.
  if(!target.enabled){if(!add(target.guid,0,Value{0,1})||!commit(net,store,j))return response(1,L"目标网卡启用失败，已尝试恢复");}
  bool ready=false;
  for(int i=0;i<30;++i){auto refreshed=net.wmi.list();for(auto& a:refreshed)if(_wcsicmp(a.guid,target.guid)==0&&a.enabled&&a.connected&&net.ready(target.guid))ready=true;if(ready)break;if(WaitForSingleObject(stopping,500)==WAIT_OBJECT_0)break;}
  if(!ready){recover(net,store);return response(1,L"目标网络未连接，已保留另一张网卡并尝试恢复。请先连接 Wi-Fi 或插入网线。");}
  if(other.enabled){Entry e={};wcsncpy(e.guid,other.guid,39);e.family=0;if(!net.read(e,e.before)){recover(net,store);return response(1,L"读取另一张网卡失败，已尝试恢复");}e.after={0,0};e.attempted=1;j.entries[j.count++]=e;
   // Extend the same durable transaction; commit cannot run over a journal.
   if(WaitForSingleObject(stopping,0)==WAIT_OBJECT_0||WaitForSingleObject(controller,0)==WAIT_OBJECT_0||!store.save(j)||!net.write(e,e.after)){recover(net,store);return response(1,L"禁用另一张网卡失败，已尝试恢复");}
   Value v={};bool ok=false;for(int i=0;i<20;++i){if(net.read(e,v)&&equal(v,e.after)){ok=true;break;}Sleep(250);}if(!ok){recover(net,store);return response(1,L"无法确认禁用状态，已尝试恢复");}
  }
  return response(0,L"已完成独占切换。退出并恢复时会恢复这两张网卡原来的启用状态。");
 }
 if(!j.count)return response(0,L"优先级已经符合要求");
 if(WaitForSingleObject(stopping,0)==WAIT_OBJECT_0||WaitForSingleObject(controller,0)==WAIT_OBJECT_0)return response(1,L"操作已取消，尚未写入优先级");
 if(!commit(net,store,j))return response(1,L"写入或校验失败，已尝试回滚；请点“恢复原设置”确认");
 return response(0,L"已设置网络优先级。VPN、专用路由和已有连接仍可能使用其他路径。");
}
void WINAPI serviceMain(DWORD,LPWSTR*){
 serviceHandle=RegisterServiceCtrlHandlerW(SERVICE,control);if(!serviceHandle)return;report(SERVICE_START_PENDING);stopping=CreateEventW(NULL,TRUE,FALSE,NULL);
 try{ComScope com;CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);Network net;Store store(pathJoin(stateDir(),L"restore.bin"));recover(net,store);
 std::wstring user=regRead(HKEY_LOCAL_MACHINE,REGKEY,L"OwnerSid");PSID valid=NULL;if(!ConvertStringSidToSidW(user.c_str(),&valid))throw std::runtime_error("owner SID");LocalFree(valid);
 std::wstring acl=L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;"+user+L")";PSECURITY_DESCRIPTOR sd=NULL;if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&sd,NULL))throw std::runtime_error("pipe ACL");SECURITY_ATTRIBUTES sa={sizeof(sa),sd,FALSE};report(SERVICE_RUNNING);
 while(WaitForSingleObject(stopping,0)!=WAIT_OBJECT_0){
  HANDLE pipe=CreateNamedPipeW(PIPE,PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|0x8,1,sizeof(Reply),sizeof(Request),0,&sa);
  if(pipe==INVALID_HANDLE_VALUE)break;
  OVERLAPPED o={};o.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);BOOL connected=ConnectNamedPipe(pipe,&o);DWORD error=connected?ERROR_SUCCESS:GetLastError();
  HANDLE events[3]={stopping,o.hEvent,controller};DWORD count=controller?3:2;
  if(error==ERROR_PIPE_CONNECTED)SetEvent(o.hEvent);
  if(error!=ERROR_PIPE_CONNECTED&&error!=ERROR_IO_PENDING&&error!=ERROR_SUCCESS){CloseHandle(o.hEvent);CloseHandle(pipe);break;}
  DWORD wait=WaitForMultipleObjects(count,events,FALSE,INFINITE);
  if(wait!=WAIT_OBJECT_0+1){CancelIo(pipe);DWORD n;GetOverlappedResult(pipe,&o,&n,TRUE);CloseHandle(o.hEvent);CloseHandle(pipe);if(wait==WAIT_OBJECT_0+2){recover(net,store);releaseController();continue;}break;}
  CloseHandle(o.hEvent);Request q={};Reply r=response(1,L"请求无效");ULONG pid=0;
  if(transfer(pipe,&q,sizeof(q),false)&&q.magic==MAGIC&&GetNamedPipeClientProcessId(pipe,&pid)){
   q.wifi[39]=q.ethernet[39]=0;
   // Check the token user, not merely the process-supplied identifier.
   HANDLE token=NULL;bool authorized=false;
   if(ImpersonateNamedPipeClient(pipe)){
    if(OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&token)){DWORD size=0;GetTokenInformation(token,TokenUser,NULL,0,&size);std::vector<BYTE>b(size);if(GetTokenInformation(token,TokenUser,b.data(),size,&size)){LPWSTR s=NULL;if(ConvertSidToStringSidW(((TOKEN_USER*)b.data())->User.Sid,&s)){authorized=user==s;LocalFree(s);}}CloseHandle(token);}RevertToSelf();
   }
   if(authorized){try{r=execute(net,store,q,pid);}catch(...){r=response(1,L"系统网络接口暂时不可用，操作已中止；如有修改请重试恢复");}}else r=response(1,L"当前用户未获授权使用此服务");
  }
  transfer(pipe,&r,sizeof(r),true);DisconnectNamedPipe(pipe);CloseHandle(pipe);
 }
 recover(net,store);releaseController();LocalFree(sd);
 }catch(...){report(SERVICE_STOPPED,ERROR_SERVICE_SPECIFIC_ERROR);if(stopping)CloseHandle(stopping);return;}
 CloseHandle(stopping);report(SERVICE_STOPPED);
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int){SERVICE_TABLE_ENTRYW table[]={{const_cast<LPWSTR>(SERVICE),serviceMain},{NULL,NULL}};StartServiceCtrlDispatcherW(table);return 0;}
