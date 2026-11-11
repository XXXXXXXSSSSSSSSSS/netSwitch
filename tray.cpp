#include "common.hpp"
#include <commctrl.h>
#include "dpi.hpp"
HINSTANCE instance;HWND mainWindow;NOTIFYICONDATAW notifyIcon={};bool busy=false,exclusive=false;std::wstring wifi,ethernet;std::vector<Adapter> adapters;HANDLE networkNotify=NULL;HICON trayImage=NULL;
const wchar_t* CLASS=L"NetSwitchNative.Tray.v1";
const UINT CALLBACK_TRAY=WM_APP+1,DONE=WM_APP+2;
struct Job{Request q;bool exitAfter;};
Request request(DWORD command){Request q={};q.magic=MAGIC;q.command=command;wcsncpy(q.wifi,wifi.c_str(),39);wcsncpy(q.ethernet,ethernet.c_str(),39);return q;}
void CALLBACK networkChanged(PVOID,PMIB_IPINTERFACE_ROW,MIB_NOTIFICATION_TYPE){if(mainWindow)PostMessageW(mainWindow,WM_APP+3,0,0);}
void balloon(const std::wstring& s,bool error=false){notifyIcon.uFlags=NIF_INFO;wcsncpy(notifyIcon.szInfo,s.c_str(),255);wcsncpy(notifyIcon.szInfoTitle,APP,63);notifyIcon.dwInfoFlags=error?NIIF_ERROR:NIIF_INFO;Shell_NotifyIconW(NIM_MODIFY,&notifyIcon);}
DWORD WINAPI worker(void* p){Job* j=(Job*)p;Reply* r=new Reply;if(!rpc(j->q,*r))*r=response(1,L"后台服务未连接。请稍后重试，或重新安装修复服务。");PostMessageW(mainWindow,DONE,(j->q.command<<8)|(j->exitAfter?1:0),(LPARAM)r);delete j;return 0;}
void run(DWORD command,bool exitAfter=false){if(busy)return;busy=true;Job* j=new Job{request(command),exitAfter};HANDLE h=CreateThread(NULL,0,worker,j,0,NULL);if(h)CloseHandle(h);else{delete j;busy=false;balloon(L"无法启动操作线程",true);}}
bool startup(){return !regRead(HKEY_CURRENT_USER,RUNKEY,L"NetSwitchNative").empty();}
void setStartup(bool on){if(on)regWrite(HKEY_CURRENT_USER,RUNKEY,L"NetSwitchNative",L"\""+exePath()+L"\"");else{HKEY h;if(RegOpenKeyExW(HKEY_CURRENT_USER,RUNKEY,0,KEY_SET_VALUE,&h)==ERROR_SUCCESS){RegDeleteValueW(h,L"NetSwitchNative");RegCloseKey(h);}}}
void saveSettings(){regWrite(HKEY_CURRENT_USER,REGKEY,L"WiFi",wifi);regWrite(HKEY_CURRENT_USER,REGKEY,L"Ethernet",ethernet);regWrite(HKEY_CURRENT_USER,REGKEY,L"Exclusive",exclusive?L"1":L"0");}
HWND comboW,comboE,checkExclusive;
LRESULT CALLBACK settingsProc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(hidpi::dispatch(h,m,w,l))return 0;
 if(m==WM_CREATE){
  hidpi::Window* ui=hidpi::create(h,instance,490,340);
  auto label=[&](const wchar_t* text,int x,int y,int width){ui->add(L"STATIC",text,0,x,y,width,26);};
  label(L"选择 Wi-Fi 网卡：",18,18,450);comboW=ui->add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,18,45,450,170,101);
  label(L"选择有线网卡：",18,84,450);comboE=ui->add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,18,111,450,170,102);
  for(size_t i=0;i<adapters.size();++i){auto& a=adapters[i];std::wstring name=a.name;name+=a.enabled?L"":L"（已禁用）";
   if(a.kind==1||a.kind==3){LRESULT n=SendMessageW(comboW,CB_ADDSTRING,0,(LPARAM)name.c_str());SendMessageW(comboW,CB_SETITEMDATA,n,i);if(_wcsicmp(a.guid,wifi.c_str())==0)SendMessageW(comboW,CB_SETCURSEL,n,0);}
   if(a.kind==2||a.kind==3){LRESULT n=SendMessageW(comboE,CB_ADDSTRING,0,(LPARAM)name.c_str());SendMessageW(comboE,CB_SETITEMDATA,n,i);if(_wcsicmp(a.guid,ethernet.c_str())==0)SendMessageW(comboE,CB_SETCURSEL,n,0);}
  }
  checkExclusive=ui->add(L"BUTTON",L"启用独占模式（切换时停用另一张网卡）",BS_AUTOCHECKBOX|WS_TABSTOP,18,155,450,28,103);SendMessageW(checkExclusive,BM_SETCHECK,exclusive?BST_CHECKED:BST_UNCHECKED,0);
  label(L"默认优先模式保留两张网卡启用。",18,190,450);
  label(L"旧版禁用的网卡可在托盘菜单中重新启用。",18,216,450);
  label(L"优先级不保证 VPN 和所有已有连接立即迁移。",18,242,450);
  ui->add(L"BUTTON",L"保存",BS_DEFPUSHBUTTON|WS_TABSTOP,278,285,90,32,IDOK);
  ui->add(L"BUTTON",L"取消",WS_TABSTOP,378,285,90,32,IDCANCEL);
  ui->layout(ui->dpi);return 0;
 }
 if(m==WM_COMMAND){if(LOWORD(w)==IDCANCEL){DestroyWindow(h);return 0;}if(LOWORD(w)==IDOK){
  int a=(int)SendMessageW(comboW,CB_GETCURSEL,0,0),b=(int)SendMessageW(comboE,CB_GETCURSEL,0,0);if(a==CB_ERR||b==CB_ERR){msg(L"请分别选择两张网卡");return 0;}
  int ai=(int)SendMessageW(comboW,CB_GETITEMDATA,a,0),bi=(int)SendMessageW(comboE,CB_GETITEMDATA,b,0);if(ai==bi){msg(L"请选两张不同的网卡");return 0;}
  bool x=SendMessageW(checkExclusive,BM_GETCHECK,0,0)==BST_CHECKED;
  if(x&&!exclusive&&MessageBoxW(h,L"独占模式会停用另一张选定网卡，可能中断下载、远程连接及内网访问。退出并恢复时恢复原来的启用状态。是否开启？",APP,MB_YESNO|MB_ICONWARNING)!=IDYES)return 0;
  wifi=adapters[ai].guid;ethernet=adapters[bi].guid;exclusive=x;try{saveSettings();}catch(...){msg(L"保存失败");return 0;}DestroyWindow(h);return 0;
 }}return DefWindowProcW(h,m,w,l);
}
void settings(){HWND old=FindWindowW(L"NetSwitchNative.Settings",NULL);if(old){SetForegroundWindow(old);return;}if(adapters.empty()){msg(L"请等待网卡列表读取完成，再打开设置。");run(LIST);return;}HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,L"NetSwitchNative.Settings",L"网络切换设置",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,510,375,NULL,NULL,instance,NULL);ShowWindow(h,SW_SHOW);}
void menu(){HMENU m=CreatePopupMenu();std::wstring mode=exclusive?L"模式：独占切换":L"模式：网络优先级";AppendMenuW(m,MF_STRING|MF_GRAYED,0,mode.c_str());
 for(auto& a:adapters)if(_wcsicmp(a.guid,wifi.c_str())==0||_wcsicmp(a.guid,ethernet.c_str())==0){std::wstring s=a.name;s+=!a.enabled?L"：已禁用":a.connected?L"：已连接":L"：未连接";AppendMenuW(m,MF_STRING|MF_GRAYED,0,s.c_str());}
 AppendMenuW(m,MF_SEPARATOR,0,NULL);UINT flags=MF_STRING|(busy?MF_GRAYED:0);
 AppendMenuW(m,flags,PREFER_WIFI,exclusive?L"切换到 Wi-Fi（独占）":L"优先 Wi-Fi");AppendMenuW(m,flags,PREFER_ETH,exclusive?L"切换到有线（独占）":L"优先有线");
 AppendMenuW(m,flags,RESTORE,L"恢复原设置");AppendMenuW(m,flags,ENABLE_WIFI,L"重新启用选定 Wi-Fi 网卡");AppendMenuW(m,flags,ENABLE_ETH,L"重新启用选定有线网卡");AppendMenuW(m,MF_SEPARATOR,0,NULL);
 AppendMenuW(m,flags,30,L"设置…");AppendMenuW(m,flags,LIST,L"刷新网卡状态");AppendMenuW(m,flags|(startup()?MF_CHECKED:0),31,L"开机自启");AppendMenuW(m,flags,STATUS,L"查看恢复状态");AppendMenuW(m,flags,32,L"退出并恢复原设置");
 POINT p;GetCursorPos(&p);SetForegroundWindow(mainWindow);UINT c=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,mainWindow,NULL);DestroyMenu(m);PostMessageW(mainWindow,WM_NULL,0,0);
 if(c==30)settings();else if(c==31){try{setStartup(!startup());}catch(...){msg(L"设置开机自启失败");}}else if(c==32)run(RESTORE,true);else if(c){if(exclusive&&c==PREFER_WIFI)c=EXCLUSIVE_WIFI;if(exclusive&&c==PREFER_ETH)c=EXCLUSIVE_ETH;run(c);}
}
void refreshTrayIcon(){HWND taskbar=FindWindowW(L"Shell_TrayWnd",NULL);HICON next=hidpi::icon(instance,1,hidpi::windowDpi(taskbar?taskbar:mainWindow),true);if(!next)return;HICON old=trayImage;trayImage=next;notifyIcon.hIcon=next;notifyIcon.uFlags=NIF_ICON;Shell_NotifyIconW(NIM_MODIFY,&notifyIcon);if(old)DestroyIcon(old);}
LRESULT CALLBACK mainProc(HWND h,UINT m,WPARAM w,LPARAM l){
 static UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");if(m==taskbarCreated){notifyIcon.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;Shell_NotifyIconW(NIM_ADD,&notifyIcon);refreshTrayIcon();return 0;}
 if(m==WM_DPICHANGED||m==WM_SETTINGCHANGE||m==WM_DISPLAYCHANGE){refreshTrayIcon();return 0;}
 if(m==CALLBACK_TRAY && (l==WM_RBUTTONUP||l==WM_LBUTTONUP)){menu();return 0;}
 if(m==WM_APP+3){SetTimer(h,55,1000,NULL);return 0;}
 if(m==WM_TIMER&&w==55){KillTimer(h,55);if(!busy)run(LIST);else SetTimer(h,55,1000,NULL);return 0;}
 if(m==DONE){Reply* r=(Reply*)l;busy=false;if((w>>8)==LIST&&r->count<=32){adapters.assign(r->adapters,r->adapters+r->count);}else if((w>>8)!=LIST||r->code!=0)balloon(r->text,r->code!=0);
  bool exitNow=(w&1)&&r->code==0;delete r;if(exitNow)DestroyWindow(h);else if((w>>8)!=LIST)run(LIST);return 0;}
 if(m==WM_CLOSE){run(RESTORE,true);return 0;}if(m==WM_QUERYENDSESSION)return TRUE;
 if(m==WM_ENDSESSION&&w){Shell_NotifyIconW(NIM_DELETE,&notifyIcon);return 0;}
 if(m==WM_TIMER&&w==99){DestroyWindow(h);return 0;}
 if(m==WM_DESTROY){if(networkNotify)CancelMibChangeNotify2(networkNotify);networkNotify=NULL;Shell_NotifyIconW(NIM_DELETE,&notifyIcon);if(trayImage)DestroyIcon(trayImage);trayImage=NULL;PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,LPWSTR,int){instance=inst;HANDLE mutex=CreateMutexW(NULL,TRUE,L"Local\\NetSwitchNative.Tray.v1");if(!mutex)return 4;if(GetLastError()==ERROR_ALREADY_EXISTS){msg(L"网络切换已经在托盘中运行");CloseHandle(mutex);return 0;}
 wifi=regRead(HKEY_CURRENT_USER,REGKEY,L"WiFi");ethernet=regRead(HKEY_CURRENT_USER,REGKEY,L"Ethernet");exclusive=regRead(HKEY_CURRENT_USER,REGKEY,L"Exclusive")==L"1";
 WNDCLASSW c={};c.hInstance=inst;c.lpfnWndProc=mainProc;c.lpszClassName=CLASS;c.hIcon=LoadIconW(inst,MAKEINTRESOURCEW(1));RegisterClassW(&c);c.lpfnWndProc=settingsProc;c.lpszClassName=L"NetSwitchNative.Settings";c.hCursor=LoadCursor(NULL,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassW(&c);
 mainWindow=CreateWindowW(CLASS,APP,WS_OVERLAPPED,0,0,0,0,NULL,NULL,inst,NULL);if(!mainWindow){CloseHandle(mutex);return 2;}notifyIcon.cbSize=sizeof(notifyIcon);notifyIcon.hWnd=mainWindow;notifyIcon.uID=1;notifyIcon.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;notifyIcon.uCallbackMessage=CALLBACK_TRAY;notifyIcon.hIcon=LoadIconW(inst,MAKEINTRESOURCEW(1));wcsncpy(notifyIcon.szTip,L"网络切换 · 点击选择网络",127);if(!Shell_NotifyIconW(NIM_ADD,&notifyIcon)){DestroyWindow(mainWindow);CloseHandle(mutex);return 3;}
 refreshTrayIcon();
 if(wcsstr(GetCommandLineW(),L"--smoke-ui"))SetTimer(mainWindow,99,1500,NULL);else{NotifyIpInterfaceChange(AF_UNSPEC,networkChanged,NULL,FALSE,&networkNotify);run(LIST);}MSG message;while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}ReleaseMutex(mutex);CloseHandle(mutex);return 0;}
