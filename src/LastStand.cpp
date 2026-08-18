#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <string>

#ifndef PROPID
#define PROPID long
#endif

#include <gdiplus.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// 时间参数（毫秒）
const int TOTAL_DURATION = 18000;
const int FADE_IN_START = 1000;
const int FADE_IN_END = 6000;
const int FADE_OUT_START = 6000;
const int FADE_OUT_END = 10000;
const int FRAME_INTERVAL = 500;
const int TIMER_INTERVAL = 50;

const int TIMER_ID = 1;

// 全局变量
HWND g_hWnd = NULL;
UINT g_alpha = 255;
int g_imgAlpha = 0;
int g_currentFrame = 0;
bool g_animationStopped = false;
std::wstring g_exeDir;
Image* g_images[3] = {NULL, NULL, NULL};
int g_screenWidth = 0, g_screenHeight = 0;

// 双缓冲用位图
HBITMAP g_hBackBuffer = NULL;
HBITMAP g_hOldBitmap = NULL;
HDC g_hMemDC = NULL;

int g_lastImgAlpha = -1;
int g_lastFrame = -1;

// 函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
bool LoadImages();
void GetExeDirectory();

void GetExeDirectory()
{
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring path(buffer);
    size_t pos = path.find_last_of(L'\\');
    if (pos != std::wstring::npos)
        g_exeDir = path.substr(0, pos + 1);
    else
        g_exeDir = L"";
}

bool LoadImages()
{
    const wchar_t* filenames[] = {
        L"assets/images/laststand1.png",
        L"assets/images/laststand2.png",
        L"assets/images/laststand3.png"
    };
    for (int i = 0; i < 3; ++i) {
        std::wstring fullPath = g_exeDir + filenames[i];
        g_images[i] = new Image(fullPath.c_str());
        if (g_images[i]->GetLastStatus() != Ok) {
            delete g_images[i];
            g_images[i] = NULL;
            g_images[i] = new Image(filenames[i]);
            if (g_images[i]->GetLastStatus() != Ok) {
                delete g_images[i];
                g_images[i] = NULL;
                wchar_t msg[256];
                wsprintfW(msg, L"Failed to load image: %s", filenames[i]);
                MessageBoxW(NULL, msg, L"Error", MB_ICONERROR);
                return false;
            }
        }
    }
    return true;
}

// 绘制函数
void DrawFrame(HDC hdc, int w, int h)
{
    RECT rect = {0, 0, w, h};
    HBRUSH hBrush = CreateSolidBrush(RGB(0,0,0));
    FillRect(hdc, &rect, hBrush);
    DeleteObject(hBrush);

    if (g_currentFrame >= 0 && g_currentFrame < 3 && g_images[g_currentFrame] && g_imgAlpha > 0) {
        Graphics graphics(hdc);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        int imgW = g_images[g_currentFrame]->GetWidth();
        int imgH = g_images[g_currentFrame]->GetHeight();
        float ratio = std::min((float)w / imgW, (float)h / imgH);
        int dstW = (int)(imgW * ratio);
        int dstH = (int)(imgH * ratio);
        int dstX = (w - dstW) / 2;
        int dstY = (h - dstH) / 2;
        Rect dstRect(dstX, dstY, dstW, dstH);

        ImageAttributes imgAttr;
        ColorMatrix cm = {
            1.0f, 0, 0, 0, 0,
            0, 1.0f, 0, 0, 0,
            0, 0, 1.0f, 0, 0,
            0, 0, 0, (float)g_imgAlpha / 255.0f, 0,
            0, 0, 0, 0, 1.0f
        };
        imgAttr.SetColorMatrix(&cm);
        graphics.DrawImage(g_images[g_currentFrame], dstRect, 0, 0, imgW, imgH, UnitPixel, &imgAttr);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CREATE: {
            g_screenWidth = GetSystemMetrics(SM_CXSCREEN);
            g_screenHeight = GetSystemMetrics(SM_CYSCREEN);

            // 创建双缓冲内存DC和位图
            HDC hdc = GetDC(hwnd);
            g_hMemDC = CreateCompatibleDC(hdc);
            g_hBackBuffer = CreateCompatibleBitmap(hdc, g_screenWidth, g_screenHeight);
            g_hOldBitmap = (HBITMAP)SelectObject(g_hMemDC, g_hBackBuffer);
            ReleaseDC(hwnd, hdc);

            SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
            SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL);

            std::wstring audioPath = g_exeDir + L"assets/audio/Laststanddrone.wav";
            PlaySoundW(audioPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
            if (GetLastError() == 2) {
                PlaySoundW(L"assets/audio/Laststanddrone.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawFrame(g_hMemDC, g_screenWidth, g_screenHeight);
            BitBlt(hdc, 0, 0, g_screenWidth, g_screenHeight, g_hMemDC, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_TIMER: {
            static DWORD startTime = GetTickCount();
            DWORD elapsed = GetTickCount() - startTime;
            if (elapsed > TOTAL_DURATION) elapsed = TOTAL_DURATION;

            UINT newAlpha = 255;
            if (elapsed >= FADE_OUT_START && elapsed < FADE_OUT_END) {
                UINT fadeOutElapsed = elapsed - FADE_OUT_START;
                UINT fadeOutTotal = FADE_OUT_END - FADE_OUT_START;
                newAlpha = (UINT)(255 * (fadeOutTotal - fadeOutElapsed) / fadeOutTotal);
                if (newAlpha < 0) newAlpha = 0;
            } else if (elapsed >= FADE_OUT_END) {
                newAlpha = 0;
            }
            if (newAlpha != g_alpha) {
                g_alpha = newAlpha;
                SetLayeredWindowAttributes(hwnd, 0, g_alpha, LWA_ALPHA);
            }

            int newImgAlpha = 0;
            if (elapsed >= FADE_IN_START && elapsed < FADE_IN_END) {
                UINT fadeInElapsed = elapsed - FADE_IN_START;
                UINT fadeInTotal = FADE_IN_END - FADE_IN_START;
                newImgAlpha = (UINT)(255 * fadeInElapsed / fadeInTotal);
                if (newImgAlpha > 255) newImgAlpha = 255;
            } else if (elapsed >= FADE_IN_END) {
                newImgAlpha = 255;
            }
            if (newImgAlpha != g_imgAlpha) {
                g_imgAlpha = newImgAlpha;
            }

            int newFrame = g_currentFrame;
            if (elapsed < FADE_IN_END) {
                newFrame = (elapsed / FRAME_INTERVAL) % 3;
            } else {
                if (!g_animationStopped) {
                    g_animationStopped = true;
                }
            }

            bool needRedraw = false;
            if (newImgAlpha != g_lastImgAlpha) {
                g_lastImgAlpha = newImgAlpha;
                needRedraw = true;
            }
            if (newFrame != g_lastFrame) {
                g_lastFrame = newFrame;
                g_currentFrame = newFrame;
                needRedraw = true;
            }
            if (needRedraw) {
                InvalidateRect(hwnd, NULL, FALSE);
            }

            if (elapsed >= TOTAL_DURATION) {
                KillTimer(hwnd, TIMER_ID);
                PostQuitMessage(0);
            }
            break;
        }
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            break;
        }
        case WM_DESTROY: {
            KillTimer(hwnd, TIMER_ID);
            if (g_hMemDC) {
                SelectObject(g_hMemDC, g_hOldBitmap);
                DeleteObject(g_hBackBuffer);
                DeleteDC(g_hMemDC);
            }
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    HANDLE hMutex = CreateMutexA(NULL, FALSE, "Local\\LastStandApp_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    GetExeDirectory();

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
        CloseHandle(hMutex);
        return 1;
    }

    if (!LoadImages()) {
        GdiplusShutdown(gdiplusToken);
        CloseHandle(hMutex);
        return 1;
    }

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"LastStandClass";
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        L"LastStandClass",
        L"LastStand",
        WS_POPUP,
        0, 0, screenW, screenH,
        NULL, NULL, hInstance, NULL
    );
    if (!hwnd) {
        for (int i=0;i<3;i++) if(g_images[i]) delete g_images[i];
        GdiplusShutdown(gdiplusToken);
        CloseHandle(hMutex);
        return 1;
    }
    g_hWnd = hwnd;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    for (int i=0;i<3;i++) if(g_images[i]) delete g_images[i];
    GdiplusShutdown(gdiplusToken);
    CloseHandle(hMutex);
    return 0;
}
