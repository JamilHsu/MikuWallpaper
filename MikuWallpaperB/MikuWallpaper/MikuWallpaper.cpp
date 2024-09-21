// This program must declare DPI awareness
#include <windows.h>
#include "resource.h"
#include<iostream>
bool IsWindowVisibleInRect(HWND hwnd, RECT rect);
HWND FindDesktopWindow();
HWND get_wallpaper_window(){
    // Fetch the Progman window
    HWND progman = GetShellWindow();
    // Send 0x052C to Progman. This message directs Progman to spawn a
    // WorkerW behind the desktop icons. If it is already there, nothing
    // happens.
    SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);

    HWND wallpaper_hwnd = GetWindow(progman, GW_HWNDPREV);
    wchar_t buffer[128];
    GetClassNameW(wallpaper_hwnd, buffer, 128);
    if (wcscmp(buffer, L"WorkerW")) {
        return nullptr;// Error!!!
    }
    // Return the handle you're looking for.
    return wallpaper_hwnd;
}
int main(){
    HWND wallpaper = get_wallpaper_window();
    if (wallpaper == nullptr) {
        std::cerr << "\aFailed to get wallpaper window!";
        system("pause");
        return -1;
    }
    HWND desktop = FindDesktopWindow();
    if (wallpaper == nullptr) {
        std::cerr << "\aFailed to find desktop window!";
        system("pause");
        return -1;
    }
    //隱藏主控台窗口
    {
        HWND hwnd = GetConsoleWindow();
        Sleep(1);//確保主控台視窗初始化完成，以免GetWindow失敗
        HWND owner = GetWindow(hwnd, GW_OWNER);
        if (owner == NULL) {
            ShowWindow(hwnd, SW_HIDE);
        }
        else {
            ShowWindow(owner, SW_HIDE);
        }
    }
    HDC hdc = GetDC(wallpaper);
    HDC memDC = CreateCompatibleDC(hdc);
    SetStretchBltMode(memDC, HALFTONE);
    struct IMAGES {
        DWORD duration;
        HBITMAP image = NULL;
        ~IMAGES() {
            DeleteObject(image);
        }
    }images[6]{ {8 * 30},{30},{30},{30},{2 * 30},{2 * 30} };//用於組成動畫的6張點陣圖 // 6 bitmaps used for composing animation
    const int rc[6]{ IDB_BITMAP1 ,IDB_BITMAP2 ,IDB_BITMAP3 ,IDB_BITMAP4 ,IDB_BITMAP5 ,IDB_BITMAP6 };
    const SIZE rcbitmapsize = { 1920,1440 };//此處的數值是"我的"點陣圖的尺寸 // These values are the dimensions of "my" bitmaps
    int windowWidth, windowHeight;
    {
        RECT rect;
        GetWindowRect(wallpaper, &rect);
        windowWidth = rect.right - rect.left;
        windowHeight = rect.bottom - rect.top;
    }
    //資源點陣圖將會預先拉伸到此尺寸，這也同時是窗口的大小 // The resource bitmaps will be pre-stretched to this size, which is also the size of the window
    const SIZE bitmapsize = ((double)rcbitmapsize.cx / rcbitmapsize.cy) > ((double)windowWidth / windowHeight) ?
        SIZE{ windowWidth, MulDiv(rcbitmapsize.cy, windowWidth, rcbitmapsize.cx) } : SIZE{ MulDiv(rcbitmapsize.cx, windowHeight, rcbitmapsize.cy),windowHeight };
    {
        HDC tempDC = CreateCompatibleDC(hdc);
        //從資源中載入點陣圖並預先拉伸到bitmapsize // Load bitmaps from resources and pre-stretch them to bitmapsize
        for (int i = 0; i < 6; ++i) {
            HBITMAP temp = (HBITMAP)LoadImage(
                GetModuleHandle(NULL),
                MAKEINTRESOURCE(rc[i]),
                IMAGE_BITMAP,
                0, 0,
                LR_DEFAULTSIZE
            );
            auto stockbitmap = SelectObject(tempDC, temp);
            images[i].image = CreateCompatibleBitmap(hdc, bitmapsize.cx, bitmapsize.cy);
            SelectObject(memDC, images[i].image);
            StretchBlt(memDC, 0, 0, bitmapsize.cx, bitmapsize.cy, tempDC, 0, 0, rcbitmapsize.cx, rcbitmapsize.cy, SRCCOPY);
            SelectObject(tempDC, stockbitmap);
            DeleteObject(temp);
        }
        DeleteDC(tempDC);
    }
    ReleaseDC(wallpaper, hdc);
    int x = (windowWidth - bitmapsize.cx) / 2;
    int y = (windowHeight - bitmapsize.cy) / 2;
    const RECT rcWindow{ x,y,x + bitmapsize.cx ,y + bitmapsize.cy };
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    while (true) {
        for (int i = 0; i < 6; ++i) {
            //檢查窗口是否可見，如果不可見，則不進行繪製以減少資源消耗
            //預先將用於檢查窗口可見性的矩形排除掉被工作列覆蓋的範圍
            RECT rcTaskbar;
            GetWindowRect(taskbar, &rcTaskbar);
            RECT rect;
            SubtractRect(&rect, &rcWindow, &rcTaskbar);
            if (IsWindowVisibleInRect(desktop, rect)) {
                SelectObject(memDC, images[i].image);
                hdc = GetDC(wallpaper);
                BitBlt(hdc, x, y, bitmapsize.cx, bitmapsize.cy, memDC, 0, 0, SRCCOPY);
                ReleaseDC(wallpaper, hdc);
            }
            Sleep(images[i].duration);
        }
    }
    DeleteDC(memDC);
    return 0;
}
//透過檢查四個角落與四條邊中間的點檢查窗口是否可見，因此不是很可靠。已知當使用者按下Alt + tab或⊞Win + tab時始終會傳會false
bool IsWindowVisibleInRect(HWND hwnd, RECT rect) {
    return
        WindowFromPoint({ rect.left,rect.top }) == hwnd ||
        WindowFromPoint({ rect.right - 1,rect.top }) == hwnd ||
        WindowFromPoint({ rect.left,rect.bottom - 1 }) == hwnd ||
        WindowFromPoint({ rect.right - 1,rect.bottom - 1 }) == hwnd ||
        WindowFromPoint({ (rect.left + rect.right) / 2,rect.top }) == hwnd ||
        WindowFromPoint({ (rect.left + rect.right) / 2,rect.bottom - 1 }) == hwnd ||
        WindowFromPoint({ rect.left,(rect.top + rect.bottom) / 2 }) == hwnd ||
        WindowFromPoint({ rect.right - 1,(rect.top + rect.bottom) / 2 }) == hwnd;
}
//由下往上搜尋桌面的窗口
HWND FindDesktopWindow() {
    HWND now = GetShellWindow();// Program Manager
    for (int i = 0; i < 10; ++i) {
        HWND SHELLDLL_DefView = FindWindowExW(now, NULL, L"SHELLDLL_DefView", NULL);
        if (SHELLDLL_DefView) {
            return FindWindowExW(SHELLDLL_DefView, NULL, L"SysListView32", L"FolderView");
        }
        else {
            now = GetWindow(now, GW_HWNDPREV);
        }
    }
    return nullptr;
}