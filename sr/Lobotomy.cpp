#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <math.h>
#include <string>
#pragma comment(lib, "winmm.lib")

// 全局变量
HBITMAP      g_hBitmap = NULL;
int          g_screenWidth  = 0;
int          g_screenHeight = 0;
int          g_alpha = 255;
const int    FADE_STEPS = 200;
const int    FADE_INTERVAL = 50;
UINT_PTR     g_timerId = 1;

// 参数
const int    FILTER_ALPHA = 80;
const COLORREF FILTER_COLOR = RGB(180, 50, 200);
const int    VIGNETTE_MAX_ALPHA = 150;
const int    CHROMATIC_OFFSET_MAX = 4;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
std::string GetExeDirectoryA();

std::string GetExeDirectoryA()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    size_t pos = path.find_last_of('\\');
    if (pos != std::string::npos)
        return path.substr(0, pos + 1);
    else
        return "";
}

// 截图
HBITMAP CaptureScreen()
{
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    g_screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    g_screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, g_screenWidth, g_screenHeight);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, g_screenWidth, g_screenHeight, hdcScreen, 0, 0, SRCCOPY);

    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    return hBitmap;
}

// 色散
HBITMAP CreateChromaticBitmap(HBITMAP hSrc, int w, int h)
{
    BITMAP bmp;
    GetObject(hSrc, sizeof(BITMAP), &bmp);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dstBits = NULL;
    HDC hdcScreen = GetDC(NULL);
    HBITMAP hDst = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &dstBits, NULL, 0);
    ReleaseDC(NULL, hdcScreen);
    if (!hDst || !dstBits) return NULL;

    HDC hdcMem = CreateCompatibleDC(NULL);
    SelectObject(hdcMem, hSrc);
    BITMAPINFO bmiSrc = {0};
    bmiSrc.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmiSrc.bmiHeader.biWidth = w;
    bmiSrc.bmiHeader.biHeight = -h;
    bmiSrc.bmiHeader.biPlanes = 1;
    bmiSrc.bmiHeader.biBitCount = 32;
    bmiSrc.bmiHeader.biCompression = BI_RGB;
    unsigned char* srcBits = (unsigned char*)malloc(w * h * 4);
    if (!srcBits) {
        DeleteDC(hdcMem);
        DeleteObject(hDst);
        return NULL;
    }
    int result = GetDIBits(hdcMem, hSrc, 0, h, srcBits, &bmiSrc, DIB_RGB_COLORS);
    if (result == 0) {
        free(srcBits);
        DeleteDC(hdcMem);
        DeleteObject(hDst);
        return NULL;
    }
    DeleteDC(hdcMem);

    float cx = w / 2.0f;
    float cy = h / 2.0f;
    float rx = w / 2.0f;
    float ry = h / 2.0f;
    float inv_rx2 = 1.0f / (rx * rx);
    float inv_ry2 = 1.0f / (ry * ry);

    unsigned char* dst = (unsigned char*)dstBits;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx;
            float dy = y - cy;
            float dist2 = dx * dx * inv_rx2 + dy * dy * inv_ry2;
            if (dist2 > 1.0f) dist2 = 1.0f;
            int offset = (int)(dist2 * CHROMATIC_OFFSET_MAX);
            
            int srcIdx = (y * w + x) * 4;
            int rx_src = x + offset;
            int bx_src = x - offset;
            if (rx_src < 0) rx_src = 0;
            if (rx_src >= w) rx_src = w - 1;
            if (bx_src < 0) bx_src = 0;
            if (bx_src >= w) bx_src = w - 1;
            int idxR = (y * w + rx_src) * 4;
            int idxB = (y * w + bx_src) * 4;
            int idxG = srcIdx;

            dst[srcIdx + 0] = srcBits[idxB + 0];
            dst[srcIdx + 1] = srcBits[idxG + 1];
            dst[srcIdx + 2] = srcBits[idxR + 2];
            dst[srcIdx + 3] = 255;
        }
    }

    free(srcBits);
    return hDst;
}

HBITMAP CreateVignetteBitmap(int w, int h)
{
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HDC hdcScreen = GetDC(NULL);
    HBITMAP hbm = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, hdcScreen);
    if (!hbm || !bits) return NULL;

    float cx = w / 2.0f;
    float cy = h / 2.0f;
    float rx = w / 2.0f;
    float ry = h / 2.0f;
    float inv_rx2 = 1.0f / (rx * rx);
    float inv_ry2 = 1.0f / (ry * ry);

    unsigned char* pixel = (unsigned char*)bits;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float dx = x - cx;
            float dy = y - cy;
            float dist2 = dx * dx * inv_rx2 + dy * dy * inv_ry2;
            if (dist2 > 1.0f) dist2 = 1.0f;
            float alpha = dist2 * VIGNETTE_MAX_ALPHA;
            if (alpha > 255) alpha = 255;
            unsigned char a = (unsigned char)alpha;
            pixel[0] = 0;
            pixel[1] = 0;
            pixel[2] = 0;
            pixel[3] = a;
            pixel += 4;
        }
    }
    return hbm;
}

// 绘制
void DrawScreenshotWithEffects(HDC hdc, int w, int h)
{
    if (!g_hBitmap) return;

    HDC hdcMem = CreateCompatibleDC(hdc);
    SelectObject(hdcMem, g_hBitmap);
    BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
    DeleteDC(hdcMem);

    HDC hdcFilter = CreateCompatibleDC(hdc);
    HBITMAP hbmFilter = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(hdcFilter, hbmFilter);
    HBRUSH hBrush = CreateSolidBrush(FILTER_COLOR);
    RECT rect = {0, 0, w, h};
    FillRect(hdcFilter, &rect, hBrush);
    DeleteObject(hBrush);
    BLENDFUNCTION blendFilter = { AC_SRC_OVER, 0, (BYTE)FILTER_ALPHA, 0 };
    AlphaBlend(hdc, 0, 0, w, h, hdcFilter, 0, 0, w, h, blendFilter);
    DeleteDC(hdcFilter);
    DeleteObject(hbmFilter);

    HBITMAP hbmVignette = CreateVignetteBitmap(w, h);
    if (hbmVignette)
    {
        HDC hdcVignette = CreateCompatibleDC(hdc);
        SelectObject(hdcVignette, hbmVignette);
        BLENDFUNCTION blendVignette = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        AlphaBlend(hdc, 0, 0, w, h, hdcVignette, 0, 0, w, h, blendVignette);
        DeleteDC(hdcVignette);
        DeleteObject(hbmVignette);
    }
}

// 窗口
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawScreenshotWithEffects(hdc, g_screenWidth, g_screenHeight);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_TIMER:
        {
            g_alpha -= (255 / FADE_STEPS);
            if (g_alpha <= 0)
            {
                g_alpha = 0;
                KillTimer(hwnd, g_timerId);
                DestroyWindow(hwnd);
            }
            else
            {
                SetLayeredWindowAttributes(hwnd, 0, (BYTE)g_alpha, LWA_ALPHA);
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// 主程序
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    HANDLE hMutex = CreateMutexA(NULL, FALSE, "Local\\ScreenshotFadeApp_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hMutex);
        return 0;
    }

    HBITMAP hOriginal = CaptureScreen();
    if (!hOriginal)
    {
        CloseHandle(hMutex);
        return 1;
    }

    g_hBitmap = CreateChromaticBitmap(hOriginal, g_screenWidth, g_screenHeight);
    DeleteObject(hOriginal);
    if (!g_hBitmap)
    {
        CloseHandle(hMutex);
        return 1;
    }

    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = "ScreenshotFadeClass";
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        "ScreenshotFadeClass",
        "ScreenshotFade",
        WS_POPUP,
        0, 0, g_screenWidth, g_screenHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd)
    {
        DeleteObject(g_hBitmap);
        CloseHandle(hMutex);
        return 1;
    }

    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    std::string exePath = GetExeDirectoryA();
    
    std::string audioPath = exePath + "assets/audio/Lobotomy.WAV";
    if (GetFileAttributesA(audioPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        audioPath = "assets/audio/Lobotomy.WAV";
    }
    PlaySoundA(audioPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);

    SetTimer(hwnd, g_timerId, FADE_INTERVAL, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_hBitmap) DeleteObject(g_hBitmap);
    CloseHandle(hMutex);
    return 0;
}
