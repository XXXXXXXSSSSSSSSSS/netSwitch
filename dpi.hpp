#pragma once
#include "common.hpp"
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
namespace hidpi {
inline int px(int logical, UINT dpi) { return MulDiv(logical, (int)dpi, 96); }
inline UINT systemDpi() {
    HDC dc = GetDC(NULL);
    UINT dpi = dc ? (UINT)GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ReleaseDC(NULL, dc);
    return dpi ? dpi : 96;
}
inline UINT windowDpi(HWND window) {
    typedef UINT (WINAPI *Fn)(HWND);
    static Fn fn = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    UINT dpi = fn && window ? fn(window) : systemDpi();
    return dpi ? dpi : 96;
}
inline int metric(int index, UINT dpi) {
    typedef int (WINAPI *Fn)(int, UINT);
    static Fn fn = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi");
    return fn ? fn(index, dpi) : MulDiv(GetSystemMetrics(index), (int)dpi, (int)systemDpi());
}
inline HICON icon(HINSTANCE instance, int resource, UINT dpi, bool small) {
    int x = metric(small ? SM_CXSMICON : SM_CXICON, dpi);
    int y = metric(small ? SM_CYSMICON : SM_CYICON, dpi);
    return (HICON)LoadImageW(instance, MAKEINTRESOURCEW(resource), IMAGE_ICON, x, y, 0);
}
inline HFONT messageFont(UINT dpi) {
    typedef BOOL (WINAPI *Fn)(UINT, UINT, PVOID, UINT, UINT);
    static Fn fn = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi");
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    LOGFONTW font = {};
    if (fn && fn(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi)) {
        font = metrics.lfMessageFont;
    } else if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        font = metrics.lfMessageFont;
        font.lfHeight = MulDiv(font.lfHeight, (int)dpi, (int)systemDpi());
    } else {
        font.lfHeight = -MulDiv(9, dpi, 72);
        wcscpy(font.lfFaceName, L"Microsoft YaHei UI");
    }
    return CreateFontIndirectW(&font);
}
struct Control { HWND handle; int x, y, width, height; };
struct Window {
    HWND handle;
    HINSTANCE instance;
    int clientWidth, clientHeight;
    UINT dpi;
    HFONT font;
    HICON smallIcon, bigIcon;
    std::vector<Control> controls;
    Window(HWND h, HINSTANCE inst, int width, int height)
        : handle(h), instance(inst), clientWidth(width), clientHeight(height),
          dpi(windowDpi(h)), font(NULL), smallIcon(NULL), bigIcon(NULL) {}
    ~Window() {
        if (font) DeleteObject(font);
        if (smallIcon) DestroyIcon(smallIcon);
        if (bigIcon) DestroyIcon(bigIcon);
    }
    HWND add(const wchar_t* cls, const wchar_t* text, DWORD style,
             int x, int y, int width, int height, int id = 0) {
        HWND child = CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | style,
            px(x,dpi), px(y,dpi), px(width,dpi), px(height,dpi),
            handle, (HMENU)(INT_PTR)id, instance, NULL);
        controls.push_back(Control{child,x,y,width,height});
        return child;
    }
    void layout(UINT newDpi, const RECT* suggested = NULL) {
        dpi = newDpi ? newDpi : 96;
        HFONT next = messageFont(dpi);
        if (next) {
            for (const auto& c : controls) SendMessageW(c.handle, WM_SETFONT, (WPARAM)next, FALSE);
            if (font) DeleteObject(font);
            font = next;
        }
        for (const auto& c : controls)
            SetWindowPos(c.handle, NULL, px(c.x,dpi), px(c.y,dpi), px(c.width,dpi), px(c.height,dpi), SWP_NOZORDER | SWP_NOACTIVATE);
        HICON nextSmall = icon(instance,1,dpi,true), nextBig = icon(instance,1,dpi,false);
        if (nextSmall) { SendMessageW(handle,WM_SETICON,ICON_SMALL,(LPARAM)nextSmall); if(smallIcon)DestroyIcon(smallIcon);smallIcon=nextSmall; }
        if (nextBig) { SendMessageW(handle,WM_SETICON,ICON_BIG,(LPARAM)nextBig); if(bigIcon)DestroyIcon(bigIcon);bigIcon=nextBig; }
        if (suggested) {
            SetWindowPos(handle,NULL,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE);
        } else {
            RECT r={0,0,px(clientWidth,dpi),px(clientHeight,dpi)};
            typedef BOOL (WINAPI *Fn)(LPRECT,DWORD,BOOL,DWORD,UINT);
            static Fn adjust = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi");
            DWORD style=(DWORD)GetWindowLongPtrW(handle,GWL_STYLE),ex=(DWORD)GetWindowLongPtrW(handle,GWL_EXSTYLE);
            if(adjust)adjust(&r,style,FALSE,ex,dpi);else AdjustWindowRectEx(&r,style,FALSE,ex);
            SetWindowPos(handle,NULL,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        }
        RedrawWindow(handle,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    }
};
inline const wchar_t* property() { return L"NetSwitchNative.DpiWindow"; }
inline Window* get(HWND h) { return (Window*)GetPropW(h,property()); }
inline Window* create(HWND h,HINSTANCE i,int width,int height) {
    Window* ui=new Window(h,i,width,height);
    SetPropW(h,property(),ui);
    return ui;
}
inline bool dispatch(HWND h,UINT message,WPARAM w,LPARAM l) {
    Window* ui=get(h);
    if(message==WM_DPICHANGED && ui) { ui->layout(HIWORD(w),(RECT*)l); return true; }
    if(message==WM_NCDESTROY && ui) { RemovePropW(h,property()); delete ui; }
    return false;
}
}
