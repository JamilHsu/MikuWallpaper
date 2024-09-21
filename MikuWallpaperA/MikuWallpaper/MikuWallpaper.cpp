//此程式必須聲明為DPI感知
// This program must declare DPI awareness
#include<thread>
#include <Windows.h>
#include"resource.h"
HWND FindWallpaperWindow();//取得原始桌布的HWND
COLORREF GetBackgroundColor(HWND background, int width, int height);//取得桌布的顏色 // Get the color of the original wallpaper
bool IsWindowVisibleInRect(HWND hwnd, RECT rect);
std::atomic<bool> exitflag(false);//need C++20
constexpr COLORREF transparentColor = RGB(0,255,0);//所有這種顏色的像素都將變透明，因此最好找一個影像中沒有的顏色

std::thread paintThread;
//窗口繪製函式 // Window painting function
void Paint(HWND hwnd, int windowWidth, int windowHeight, HWND background, COLORREF backgroundColor) {
    HDC hdc = GetDC(hwnd);
    HDC memDC = CreateCompatibleDC(hdc);
    SetStretchBltMode(memDC, HALFTONE);

    struct IMAGES {
        DWORD duration;
        HBITMAP image = NULL;
        ~IMAGES() {
            DeleteObject(image);
        }
    }images[6]{ {8*30-3},{30-3},{30-3},{30-3},{2*30-3},{2*30-3} };//用於組成動畫的6張點陣圖 // 6 bitmaps used for composing animation
    const int rc[6]{ IDB_BITMAP1 ,IDB_BITMAP2 ,IDB_BITMAP3 ,IDB_BITMAP4 ,IDB_BITMAP5 ,IDB_BITMAP6 };//resource ID
    constexpr SIZE rcbitmapsize = { 1920,1440 };//此處的數值是"我的"點陣圖的尺寸 // These values are the dimensions of "my" bitmaps

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
    
    HDC maskDC = CreateCompatibleDC(hdc);
    ReleaseDC(hwnd, hdc);
    HBITMAP maskBitmap = CreateCompatibleBitmap(maskDC, bitmapsize.cx, bitmapsize.cy);//單色點陣圖 // Monochrome bitmap
    SelectObject(maskDC, maskBitmap);

    //窗口在螢幕上的位置 // The position of the window on the screen
    int x = (windowWidth - bitmapsize.cx) / 2;
    int y = (windowHeight - bitmapsize.cy) / 2;

    //調整大小並顯示視窗 // Resize and display the window
    SetWindowPos(hwnd, HWND_BOTTOM, x, y, bitmapsize.cx, bitmapsize.cy, SWP_NOACTIVATE | SWP_SHOWWINDOW);//窗口到此時才可見(但還沒有任何內容) // The window is visible at this point (but has no content yet)
    const RECT rcWindow{ x,y,x + bitmapsize.cx ,y + bitmapsize.cy };
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    const HBRUSH DCBrush = (HBRUSH)GetStockObject(DC_BRUSH);
    while (!exitflag) {
        for (int i = 0; i < 6 && !exitflag; ++i) {
            //檢查窗口是否可見，如果不可見，則不進行繪製以減少資源消耗
            // if window is invisible, do not draw it
            //預先將用於檢查窗口可見性的矩形排除掉被工作列覆蓋的範圍
            RECT rcTaskbar;
            GetWindowRect(taskbar, &rcTaskbar);
            RECT rect;
            SubtractRect(&rect, &rcWindow, &rcTaskbar);
            if (IsWindowVisibleInRect(background, rect)) {
                HDC backgroundDC = GetDC(background);
                //設定背景顏色，BitBlt到單色點陣圖時此顏色會轉換成0 // Set the background color, this color will convert to 0 when BitBlt to a monochrome bitmap
                SetBkColor(backgroundDC, backgroundColor);
                //製作掩碼位圖 // Create the mask bitmap
                BitBlt(maskDC, 0, 0, bitmapsize.cx, bitmapsize.cy, backgroundDC, x, y, SRCCOPY);
                //一旦使用完DC後就馬上釋放
                ReleaseDC(background, backgroundDC);

                //使用現成的點陣圖作為影像// Use ready-made bitmaps as images
                SelectObject(memDC, images[i].image);

                //在將memory DC的影像傳輸到窗口上的同時應用掩碼位圖// Apply the mask bitmap while transferring the image of the memory DC to the window
                hdc = GetDC(hwnd);
                SelectObject(hdc, DCBrush);
                SetDCBrushColor(hdc, transparentColor);
                MaskBlt(hdc, 0, 0, bitmapsize.cx, bitmapsize.cy, memDC, 0, 0, maskBitmap, 0, 0, MAKEROP4(SRCCOPY, PATCOPY));
                ReleaseDC(hwnd, hdc);
            }
            Sleep(images[i].duration);
        }
    }
    //清理資源 // Cleanup resources
    DeleteDC(maskDC);
    DeleteObject(maskBitmap);
    DeleteDC(memDC);
}

// 窗口過程函數 // Window procedure function
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        exitflag.store(true);
        paintThread.join();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"HsuOO'sWallpaperClass";
    //關閉先前的執行個體(如果存在)
    {
        HWND hwnd = FindWindowW(CLASS_NAME, nullptr);
        if (hwnd) PostMessage(hwnd, WM_CLOSE, NULL, NULL);
    }
    //Get the HWND of original wallpaper
    HWND wallpaperWindow = FindWallpaperWindow();
    if (wallpaperWindow == NULL)
    {
        MessageBox(NULL, L"Miku Wallpaper failed to start!\nReason: Unable to get original wallpaper.", NULL, MB_OK | MB_ICONERROR);
        return 0;
    }

    // 註冊窗口類 // Register the window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = NULL; //不繪製背景 // No background painting
    RegisterClassW(&wc);

    // 創建窗口 // Create the window
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE , // 擴展窗口樣式 // Extended window styles
        CLASS_NAME,                         // 窗口類名 // Window class name
        L"Miku wallpaper",                  // 窗口標題 // Window title
        WS_POPUP,                           // 窗口樣式 // Window style
        0, 0,                               // 窗口位置 // Window position
        320, 240,                           // 窗口大小 // Window size
        NULL,                               // 父窗口 // Parent window
        NULL,                               // 菜單 // Menu
        hInstance,                          // 實例句柄 // Instance handle
        NULL                                // 附加參數 // Additional parameters
    );
    if (hwnd == NULL)
    {
        MessageBox(NULL, L"Miku Wallpaper failed to start!\nReason: Unable to create window.", NULL, MB_OK| MB_ICONERROR);
        return 0;
    }

    // 設置窗口透明度 // Set the window transparency
    SetLayeredWindowAttributes(hwnd, transparentColor, NULL, LWA_COLORKEY);//設置指定顏色為透明像素// Sets the specified color to transparent pixels

    //沒有ShowWindow，因此窗口此時仍不可見 // There's no ShowWindow, so the window is still invisible at this point

    RECT rect;
    GetWindowRect(wallpaperWindow, &rect);
    COLORREF backgroundColor = GetBackgroundColor(wallpaperWindow, rect.right - rect.left, rect.bottom - rect.top);
    if (backgroundColor == CLR_INVALID) {
        MessageBox(NULL, L"Miku Wallpaper failed to start!\nReason: Unable to get background color.", NULL, MB_OK | MB_ICONERROR);
        return 0;
    }
    //使用獨立的執行緒來負責顯示影像的工作 // Use a separate thread to handle the image display work
    paintThread = std::thread(Paint, hwnd, rect.right-rect.left, rect.bottom-rect.top, wallpaperWindow, backgroundColor);

    // 消息循環 // Message pump
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
//取得桌布的顏色。傳回桌布在四個角落中出現頻率最高的顏色或左上角的顏色
// 如果四個角落的顏色都不同或取色失敗，將傳回CLR_INVALID
COLORREF GetBackgroundColor(HWND background, int width, int height) {
    HDC hdc = GetDC(background);
    COLORREF LT = GetPixel(hdc, 0, 0);
    COLORREF LB = GetPixel(hdc, 0, height - 1);
    COLORREF RT = GetPixel(hdc, width - 1, 0);
    COLORREF RB = GetPixel(hdc, width - 1, height - 1);
    ReleaseDC(background, hdc);
    if ((LT == LB) + (LT == RT) + (LT == RB) > 0) {
        return LT;
    }
    if ((LB == RT) || (LB == RB)) {
        return LB;
    }
    if (RT == RB) {
        return RT;
    }
    return CLR_INVALID;
}
//由下往上搜尋原始桌布的窗口
HWND FindWallpaperWindow() {
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
//透過檢查四個角落與四條邊中間的點檢查窗口是否可見，因此不是很可靠。當使用者按下Alt + tab或⊞Win + tab時始終會傳會false
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
