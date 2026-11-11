#pragma once
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sddl.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <wbemidl.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
const wchar_t* const SERVICE=L"NetSwitchNative";
const wchar_t* const PIPE=L"\\\\.\\pipe\\NetSwitchNative.v1";
const wchar_t* const APP=L"网络切换";
const DWORD MAGIC=0x3153574e;
inline std::wstring pathJoin(const std::wstring& a,const std::wstring& b){return a+L"\\"+b;}
inline std::wstring folder(int id){wchar_t p[MAX_PATH];if(FAILED(SHGetFolderPathW(NULL,id,NULL,0,p)))throw std::runtime_error("folder");return p;}
inline std::wstring exePath(){wchar_t p[32768];DWORD n=GetModuleFileNameW(NULL,p,32768);return std::wstring(p,n);}
inline std::wstring parent(const std::wstring& p){return p.substr(0,p.find_last_of(L"\\/"));}
inline std::wstring stateDir(){return pathJoin(folder(CSIDL_COMMON_APPDATA),L"NetSwitchNative");}
inline std::wstring installDir(){return pathJoin(folder(CSIDL_PROGRAM_FILES),L"NetSwitchNative");}
inline void msg(const std::wstring& s,UINT flags=MB_OK){MessageBoxW(NULL,s.c_str(),APP,flags);}
inline std::wstring sid(){HANDLE t=NULL; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&t))throw std::runtime_error("token");DWORD n=0;GetTokenInformation(t,TokenUser,NULL,0,&n);std::vector<BYTE>b(n);if(!GetTokenInformation(t,TokenUser,b.data(),n,&n)){CloseHandle(t);throw std::runtime_error("token user");}LPWSTR s=NULL;ConvertSidToStringSidW(((TOKEN_USER*)b.data())->User.Sid,&s);std::wstring out=s?s:L"";if(s)LocalFree(s);CloseHandle(t);return out;}
inline bool guidValid(const wchar_t* s){GUID g;return s && wcslen(s)==38 && SUCCEEDED(CLSIDFromString(const_cast<LPWSTR>(s),&g));}
inline std::wstring regRead(HKEY root,const wchar_t* key,const wchar_t* name){HKEY h; if(RegOpenKeyExW(root,key,0,KEY_READ|KEY_WOW64_64KEY,&h)!=ERROR_SUCCESS)return L"";wchar_t b[2048]={0};DWORD n=sizeof(b),t;LONG e=RegQueryValueExW(h,name,0,&t,(BYTE*)b,&n);RegCloseKey(h);return e==ERROR_SUCCESS && t==REG_SZ ? std::wstring(b):L"";}
inline void regWrite(HKEY root,const wchar_t* key,const wchar_t* name,const std::wstring& v){HKEY h;if(RegCreateKeyExW(root,key,0,NULL,0,KEY_WRITE|KEY_WOW64_64KEY,NULL,&h,NULL)!=ERROR_SUCCESS)throw std::runtime_error("registry create");LONG e=RegSetValueExW(h,name,0,REG_SZ,(BYTE*)v.c_str(),(DWORD)((v.size()+1)*2));RegCloseKey(h);if(e!=ERROR_SUCCESS)throw std::runtime_error("registry write");}
const wchar_t* const REGKEY=L"Software\\NetSwitchNative";
const wchar_t* const RUNKEY=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
struct Adapter{wchar_t guid[40],name[260];DWORD kind,enabled,connected;};
struct Request{DWORD magic,command;wchar_t wifi[40],ethernet[40];};
struct Reply{DWORD magic,code,count;wchar_t text[512];Adapter adapters[32];};
enum Command{LIST=1,PREFER_WIFI,PREFER_ETH,EXCLUSIVE_WIFI,EXCLUSIVE_ETH,RESTORE,ENABLE_WIFI,ENABLE_ETH,STATUS};
inline Reply response(DWORD code,const std::wstring& text){Reply r={};r.magic=MAGIC;r.code=code;wcsncpy(r.text,text.c_str(),511);return r;}
inline bool transfer(HANDLE h,void* p,DWORD size,bool writing){OVERLAPPED o={};o.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);DWORD done=0;BOOL ok=writing?WriteFile(h,p,size,&done,&o):ReadFile(h,p,size,&done,&o);if(!ok && GetLastError()==ERROR_IO_PENDING){if(WaitForSingleObject(o.hEvent,5000)==WAIT_OBJECT_0)ok=GetOverlappedResult(h,&o,&done,FALSE);else{CancelIo(h);GetOverlappedResult(h,&o,&done,TRUE);ok=FALSE;}}CloseHandle(o.hEvent);return ok && done==size;}
inline bool rpc(const Request& q,Reply& r){if(!WaitNamedPipeW(PIPE,3000))return false;HANDLE h=CreateFileW(PIPE,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,NULL);if(h==INVALID_HANDLE_VALUE)return false;bool ok=transfer(h,(void*)&q,sizeof(q),true)&&transfer(h,&r,sizeof(r),false)&&r.magic==MAGIC;CloseHandle(h);return ok;}
