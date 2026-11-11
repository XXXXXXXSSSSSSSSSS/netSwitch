// Exercise the actual setup and settings window procedures without installation,
// registry writes, service communication, or network changes.
#define wWinMain trayProgramEntry
#include "tray.cpp"
#undef wWinMain
#define instance setupInstance
#define wWinMain setupProgramEntry
#include "setup.cpp"
#undef wWinMain
#undef instance
#include <cassert>
#include <iostream>
void inspectLayout(HWND h,UINT dpi) {
    hidpi::Window* ui=hidpi::get(h);
    assert(ui && ui->dpi==dpi && ui->font);
    RECT client;GetClientRect(h,&client);
    LOGFONTW font={};assert(GetObjectW(ui->font,sizeof(font),&font));
    assert(font.lfHeight<0);
    for(const auto& c:ui->controls) {
        assert(c.handle && IsWindow(c.handle));
        assert((HFONT)SendMessageW(c.handle,WM_GETFONT,0,0)==ui->font);
        RECT r;GetWindowRect(c.handle,&r);MapWindowPoints(NULL,h,(POINT*)&r,2);
        assert(r.left==hidpi::px(c.x,dpi));assert(r.top==hidpi::px(c.y,dpi));
        assert(r.right-r.left==hidpi::px(c.width,dpi));
        assert(r.right<=client.right && r.bottom<=client.bottom);
    }
    ICONINFO ii={};assert(ui->smallIcon && GetIconInfo(ui->smallIcon,&ii));
    BITMAP bitmap={};assert(ii.hbmColor && GetObjectW(ii.hbmColor,sizeof(bitmap),&bitmap));
    assert(bitmap.bmWidth==hidpi::metric(SM_CXSMICON,dpi));
    DeleteObject(ii.hbmColor);DeleteObject(ii.hbmMask);
}
void exercise(HINSTANCE inst,WNDPROC proc,const wchar_t* name,int width,int height) {
    WNDCLASSW c={};c.hInstance=inst;c.lpfnWndProc=proc;c.lpszClassName=name;
    assert(RegisterClassW(&c));
    HWND h=CreateWindowW(name,L"DPI acceptance (hidden)",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,0,0,100,100,NULL,NULL,inst,NULL);
    assert(h);hidpi::Window* ui=hidpi::get(h);assert(ui);
    UINT sequence[]={96,120,144,192,96,192,120,96};
    for(UINT dpi:sequence) {
        // Give WM_DPICHANGED a valid suggested outer rectangle with enough
        // non-client room. Tests use synthetic DPI, not display reconfiguration.
        RECT r={0,0,hidpi::px(width,dpi)+80,hidpi::px(height,dpi)+100};
        SendMessageW(h,WM_DPICHANGED,MAKELONG(dpi,dpi),(LPARAM)&r);
        inspectLayout(h,dpi);
    }
    DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int i=0;i<20;++i) {
        UINT dpi=i%2?96:192;RECT r={0,0,hidpi::px(width,dpi)+80,hidpi::px(height,dpi)+100};
        SendMessageW(h,WM_DPICHANGED,MAKELONG(dpi,dpi),(LPARAM)&r);
    }
    DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    assert(after<=before+2);
    DestroyWindow(h);
}
int main(){
    instance=GetModuleHandleW(NULL);setupInstance=instance;
    typedef HANDLE (WINAPI *ContextFn)(HWND);
    typedef BOOL (WINAPI *EqualFn)(HANDLE,HANDLE);
    ContextFn context=(ContextFn)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetWindowDpiAwarenessContext");
    EqualFn equal=(EqualFn)GetProcAddress(GetModuleHandleW(L"user32.dll"),"AreDpiAwarenessContextsEqual");
    exercise(instance,settingsProc,L"NetSwitchNative.Test.Settings",490,340);
    exercise(instance,procedure,L"NetSwitchNative.Test.Setup",505,265);
    HWND h=CreateWindowW(L"STATIC",L"",0,0,0,0,0,NULL,NULL,instance,NULL);
    assert(h);if(context && equal)assert(equal(context(h),(HANDLE)(INT_PTR)-4));
    DestroyWindow(h);
    std::cout<<"Settings and setup DPI acceptance passed: 100%, 125%, 150%, 200%, round trips, control fonts/bounds, icon sizes, GDI resource stability, PMv2 awareness. Synthetic DPI; no display/network settings changed.\n";
    return 0;
}
