#include "network.hpp"
#include "dpi.hpp"
HINSTANCE instance;HWND checkStartup;const wchar_t* SETUP_CLASS=L"NetSwitchNative.Setup";
bool extract(int id,const std::wstring& file){HRSRC r=FindResourceW(instance,MAKEINTRESOURCEW(id),RT_RCDATA);if(!r)return false;HGLOBAL g=LoadResource(instance,r);void* p=LockResource(g);DWORD size=SizeofResource(instance,r),n=0;HANDLE h=CreateFileW(file.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE)return false;bool ok=WriteFile(h,p,size,&n,NULL)&&size==n&&FlushFileBuffers(h);CloseHandle(h);return ok;}
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void stopService(SC_HANDLE service){SERVICE_STATUS s={};ControlService(service,SERVICE_CONTROL_STOP,&s);for(int i=0;i<100;++i){SERVICE_STATUS_PROCESS p={};DWORD n=0;check(QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,(BYTE*)&p,sizeof(p),&n)!=0,"service query");if(p.dwCurrentState==SERVICE_STOPPED)return;Sleep(300);}throw std::runtime_error("service stop timeout");}
void secureDir(const std::wstring& path){PSECURITY_DESCRIPTOR sd=NULL;check(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)",SDDL_REVISION_1,&sd,NULL)!=0,"ACL");SECURITY_ATTRIBUTES a={sizeof(a),sd,FALSE};BOOL made=CreateDirectoryW(path.c_str(),&a);DWORD error=GetLastError();check(made||error==ERROR_ALREADY_EXISTS,"directory");check(SetFileSecurityW(path.c_str(),DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,sd)!=0,"directory security");LocalFree(sd);}
int elevatedInstall(const std::wstring& owner){SC_HANDLE manager=NULL,service=NULL;try{
 PSID valid=NULL;check(ConvertStringSidToSidW(owner.c_str(),&valid)!=0,"owner");LocalFree(valid);
 HWND oldTray=FindWindowW(L"NetSwitchNative.Tray.v1",NULL);if(oldTray){PostMessageW(oldTray,WM_CLOSE,0,0);for(int i=0;i<70&&IsWindow(oldTray);++i)Sleep(100);check(!IsWindow(oldTray),"close tray first");}
 std::wstring dest=installDir();CreateDirectoryW(dest.c_str(),NULL);
 DWORD attrs=GetFileAttributesW(stateDir().c_str());check(attrs==INVALID_FILE_ATTRIBUTES||!(attrs&FILE_ATTRIBUTE_REPARSE_POINT),"state directory reparse");
 if(regRead(HKEY_LOCAL_MACHINE,REGKEY,L"OwnerSid").empty()){check(GetFileAttributesW(pathJoin(stateDir(),L"restore.bin").c_str())==INVALID_FILE_ATTRIBUTES,"unexpected journal");check(GetFileAttributesW(pathJoin(stateDir(),L"restore.bin.tmp").c_str())==INVALID_FILE_ATTRIBUTES,"unexpected temp journal");}
 secureDir(stateDir());
 manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT|SC_MANAGER_CREATE_SERVICE);check(manager!=NULL,"SCM");service=OpenServiceW(manager,SERVICE,SERVICE_STOP|SERVICE_QUERY_STATUS|SERVICE_CHANGE_CONFIG|SERVICE_START);
 if(service){stopService(service);Store state(pathJoin(stateDir(),L"restore.bin"));Snapshot j={};check(state.load(j)==0,"restore pending; uninstall aborted");}
 check(extract(201,pathJoin(dest,L"NetSwitch.exe")),"extract tray");check(extract(202,pathJoin(dest,L"NetSwitchService.exe")),"extract service");
 if(_wcsicmp(exePath().c_str(),pathJoin(dest,L"Setup.exe").c_str())!=0)check(CopyFileW(exePath().c_str(),pathJoin(dest,L"Setup.exe").c_str(),FALSE)!=0,"copy installer");
 regWrite(HKEY_LOCAL_MACHINE,REGKEY,L"OwnerSid",owner);
 std::wstring image=L"\""+pathJoin(dest,L"NetSwitchService.exe")+L"\"";
 if(!service)service=CreateServiceW(manager,SERVICE,L"网络切换后台服务",SERVICE_STOP|SERVICE_QUERY_STATUS|SERVICE_CHANGE_CONFIG|SERVICE_START,SERVICE_WIN32_OWN_PROCESS,SERVICE_AUTO_START,SERVICE_ERROR_NORMAL,image.c_str(),NULL,NULL,NULL,NULL,NULL);
 check(service!=NULL,"create service");check(ChangeServiceConfigW(service,SERVICE_NO_CHANGE,SERVICE_AUTO_START,SERVICE_NO_CHANGE,image.c_str(),NULL,NULL,NULL,NULL,NULL,NULL)!=0,"service config");
 SERVICE_DESCRIPTIONW description={const_cast<LPWSTR>(L"执行指定网卡切换并保留恢复记录。")};ChangeServiceConfig2W(service,SERVICE_CONFIG_DESCRIPTION,&description);
 SC_ACTION actions[2]={{SC_ACTION_RESTART,5000},{SC_ACTION_RESTART,15000}};SERVICE_FAILURE_ACTIONSW failure={86400,NULL,NULL,2,actions};ChangeServiceConfig2W(service,SERVICE_CONFIG_FAILURE_ACTIONS,&failure);
 check(StartServiceW(service,0,NULL)!=0||GetLastError()==ERROR_SERVICE_ALREADY_RUNNING,"start service");
 bool running=false;for(int i=0;i<50;++i){SERVICE_STATUS_PROCESS p={};DWORD n;check(QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,(BYTE*)&p,sizeof(p),&n)!=0,"query start");if(p.dwCurrentState==SERVICE_RUNNING){running=true;break;}if(p.dwCurrentState==SERVICE_STOPPED)break;Sleep(300);}check(running,"service did not start");
 const wchar_t* uninstall=L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NetSwitchNative";
 regWrite(HKEY_LOCAL_MACHINE,uninstall,L"DisplayName",L"网络切换");regWrite(HKEY_LOCAL_MACHINE,uninstall,L"DisplayVersion",L"1.0.1");regWrite(HKEY_LOCAL_MACHINE,uninstall,L"Publisher",L"NetSwitchNative");regWrite(HKEY_LOCAL_MACHINE,uninstall,L"UninstallString",L"\""+pathJoin(dest,L"Setup.exe")+L"\" --remove");regWrite(HKEY_LOCAL_MACHINE,uninstall,L"InstallLocation",dest);regWrite(HKEY_LOCAL_MACHINE,uninstall,L"DisplayIcon",pathJoin(dest,L"NetSwitch.exe"));
 CloseServiceHandle(service);CloseServiceHandle(manager);return 0;
 }catch(...){if(service)CloseServiceHandle(service);if(manager)CloseServiceHandle(manager);msg(L"安装未完成。可能是权限、文件占用或尚未恢复的网络设置。若曾使用旧版本，请先退出并恢复原设置后重试。",MB_OK|MB_ICONERROR);return 1;}}
int elevatedUninstall(){SC_HANDLE manager=NULL,service=NULL;try{
 HWND tray=FindWindowW(L"NetSwitchNative.Tray.v1",NULL);if(tray){PostMessageW(tray,WM_CLOSE,0,0);for(int i=0;i<50&&IsWindow(tray);++i)Sleep(100);}
 manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);check(manager!=NULL,"SCM");service=OpenServiceW(manager,SERVICE,SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE);
 if(service)stopService(service);
 Store store(pathJoin(stateDir(),L"restore.bin"));Snapshot j={};check(store.load(j)==0,"restore pending");
 if(service){check(DeleteService(service)!=0,"delete service");CloseServiceHandle(service);service=NULL;}CloseServiceHandle(manager);manager=NULL;
 std::wstring dest=installDir();DeleteFileW(pathJoin(dest,L"NetSwitch.exe").c_str());DeleteFileW(pathJoin(dest,L"NetSwitchService.exe").c_str());check(DeleteFileW(pathJoin(dest,L"Setup.exe").c_str())!=0||GetLastError()==ERROR_FILE_NOT_FOUND,"delete installer");RemoveDirectoryW(dest.c_str());
 wchar_t tmp[MAX_PATH];GetTempPathW(MAX_PATH,tmp);std::wstring running=exePath();if(_wcsnicmp(running.c_str(),tmp,wcslen(tmp))==0)MoveFileExW(running.c_str(),NULL,MOVEFILE_DELAY_UNTIL_REBOOT);
 DeleteFileW(pathJoin(stateDir(),L"restore.bin.tmp").c_str());RemoveDirectoryW(stateDir().c_str());
 RegDeleteKeyExW(HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NetSwitchNative",KEY_WOW64_64KEY,0);RegDeleteKeyExW(HKEY_LOCAL_MACHINE,REGKEY,KEY_WOW64_64KEY,0);return 0;
 }catch(...){if(service)CloseServiceHandle(service);if(manager)CloseServiceHandle(manager);msg(L"卸载已停止：网络设置尚未完全恢复或服务无法停止。程序和恢复记录已保留，请重新启动服务、接回原网卡后恢复再卸载。",MB_OK|MB_ICONERROR);return 1;}}
DWORD elevate(const std::wstring& args){SHELLEXECUTEINFOW s={};s.cbSize=sizeof(s);s.fMask=SEE_MASK_NOCLOSEPROCESS;s.lpVerb=L"runas";std::wstring path=exePath();s.lpFile=path.c_str();s.lpParameters=args.c_str();s.nShow=SW_SHOW;if(!ShellExecuteExW(&s))return 1;WaitForSingleObject(s.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(s.hProcess,&code);CloseHandle(s.hProcess);return code;}
void shortcut(){std::wstring dir=pathJoin(folder(CSIDL_PROGRAMS),L"网络切换");CreateDirectoryW(dir.c_str(),NULL);IShellLinkW* link=NULL;CoInitialize(NULL);if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(void**)&link))){link->SetPath(pathJoin(installDir(),L"NetSwitch.exe").c_str());link->SetIconLocation(pathJoin(installDir(),L"NetSwitch.exe").c_str(),0);IPersistFile* f=NULL;if(SUCCEEDED(link->QueryInterface(IID_IPersistFile,(void**)&f))){f->Save(pathJoin(dir,L"网络切换.lnk").c_str(),TRUE);f->Release();}link->Release();}CoUninitialize();}
LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l){
 if(hidpi::dispatch(h,m,w,l))return 0;
 if(m==WM_CREATE){
  hidpi::Window* ui=hidpi::create(h,instance,505,265);
  ui->add(L"STATIC",L"安装网络切换 1.0.1\n\n仅托盘运行，支持 Wi-Fi / 有线优先级和独占切换。\n安装后请先在托盘设置中选择两张网卡。",0,22,24,470,110);
  checkStartup=ui->add(L"BUTTON",L"为当前用户开机自启",BS_AUTOCHECKBOX|WS_TABSTOP,22,145,450,30);
  SendMessageW(checkStartup,BM_SETCHECK,BST_CHECKED,0);
  ui->add(L"BUTTON",L"安装",BS_DEFPUSHBUTTON|WS_TABSTOP,286,205,90,34,IDOK);
  ui->add(L"BUTTON",L"取消",WS_TABSTOP,386,205,90,34,IDCANCEL);
  ui->layout(ui->dpi);return 0;
 }

 if(m==WM_COMMAND){if(LOWORD(w)==IDCANCEL){DestroyWindow(h);return 0;}if(LOWORD(w)==IDOK){EnableWindow(h,FALSE);bool autoRun=SendMessageW(checkStartup,BM_GETCHECK,0,0)==BST_CHECKED;DWORD result=elevate(L"--install "+sid());EnableWindow(h,TRUE);if(!result){
  try{if(autoRun)regWrite(HKEY_CURRENT_USER,RUNKEY,L"NetSwitchNative",L"\""+pathJoin(installDir(),L"NetSwitch.exe")+L"\"");HKEY key;if(RegOpenKeyExW(HKEY_CURRENT_USER,RUNKEY,0,KEY_SET_VALUE,&key)==ERROR_SUCCESS){RegDeleteValueW(key,L"NetSwitchTray");RegCloseKey(key);}shortcut();}catch(...){msg(L"安装完成，但自启或快捷方式设置失败，可在托盘菜单重新设置。");}
  ShellExecuteW(NULL,L"open",pathJoin(installDir(),L"NetSwitch.exe").c_str(),NULL,NULL,SW_SHOWNORMAL);msg(L"安装完成。请点击任务栏右下角的网络切换图标，打开设置选择网卡。安装未更改网络配置。");DestroyWindow(h);
 }return 0;}}
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,LPWSTR,int){instance=inst;int argc;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);int result=0;
 if(argc==3&&wcscmp(argv[1],L"--verify-files")==0){std::wstring dest=argv[2];CreateDirectoryW(dest.c_str(),NULL);bool ok=extract(201,pathJoin(dest,L"NetSwitch.exe"))&&extract(202,pathJoin(dest,L"NetSwitchService.exe"));LocalFree(argv);return ok?0:1;}
 if(argc==2&&wcscmp(argv[1],L"--verify")==0){bool ok=true;for(int id=201;id<=202;++id){HRSRC r=FindResourceW(inst,MAKEINTRESOURCEW(id),RT_RCDATA);if(!r||SizeofResource(inst,r)<1024){ok=false;break;}HGLOBAL g=LoadResource(inst,r);const BYTE* p=(const BYTE*)LockResource(g);if(!p||p[0]!='M'||p[1]!='Z')ok=false;}LocalFree(argv);return ok?0:1;}
 if(argc==3&&wcscmp(argv[1],L"--install")==0){result=elevatedInstall(argv[2]);LocalFree(argv);return result;}
 if(argc==2&&wcscmp(argv[1],L"--uninstall")==0){LocalFree(argv);return elevatedUninstall();}
 if(argc==2&&(wcscmp(argv[1],L"--remove")==0||wcscmp(argv[1],L"--remove-temp")==0)){bool temporary=wcscmp(argv[1],L"--remove-temp")==0;LocalFree(argv);if(!temporary){if(MessageBoxW(NULL,L"卸载网络切换，并恢复程序保持的网络设置？",APP,MB_YESNO|MB_ICONQUESTION)!=IDYES)return 0;wchar_t dir[MAX_PATH],file[MAX_PATH];GetTempPathW(MAX_PATH,dir);if(!GetTempFileNameW(dir,L"NSU",0,file))return 1;DeleteFileW(file);std::wstring temp=std::wstring(file)+L".exe";if(!CopyFileW(exePath().c_str(),temp.c_str(),TRUE))return 1;HINSTANCE opened=ShellExecuteW(NULL,L"open",temp.c_str(),L"--remove-temp",NULL,SW_SHOWNORMAL);return (INT_PTR)opened>32?0:1;}Sleep(300);result=elevate(L"--uninstall");if(!result){HKEY key;if(RegOpenKeyExW(HKEY_CURRENT_USER,RUNKEY,0,KEY_SET_VALUE,&key)==ERROR_SUCCESS){RegDeleteValueW(key,L"NetSwitchNative");RegCloseKey(key);}RegDeleteKeyW(HKEY_CURRENT_USER,REGKEY);std::wstring dir=pathJoin(folder(CSIDL_PROGRAMS),L"网络切换");DeleteFileW(pathJoin(dir,L"网络切换.lnk").c_str());RemoveDirectoryW(dir.c_str());msg(L"卸载完成。临时卸载器将在下次重启后清理。");}return result;}
 LocalFree(argv);WNDCLASSW c={};c.hInstance=inst;c.lpfnWndProc=procedure;c.lpszClassName=SETUP_CLASS;c.hIcon=LoadIconW(inst,MAKEINTRESOURCEW(1));c.hCursor=LoadCursor(NULL,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassW(&c);HWND h=CreateWindowW(SETUP_CLASS,L"网络切换安装",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,525,305,NULL,NULL,inst,NULL);ShowWindow(h,SW_SHOW);MSG message;while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}return 0;}
