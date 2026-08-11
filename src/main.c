#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include "resource.h"

#ifdef HAS_NVAPI
#include <nvapi.h>
#endif

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_APPLY 1002
#define ID_TRAY_MIN_DEFAULT 1003
#define ID_TRAY_MAX_DEFAULT 1004

static HINSTANCE g_hInstance;
static HWND g_hWnd;
static NOTIFYICONDATAW g_nid = {0};
static HMENU g_hMenu;

typedef struct {
    int minClock;
    int maxClock;
    int active;
} ClockSettings;

static ClockSettings g_clocks = {0, 0, 0};

static void ShowBubble(const wchar_t *text, const wchar_t *title) {
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    StringCchCopyW(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), title);
    StringCchCopyW(nid.szInfo, ARRAYSIZE(nid.szInfo), text);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static BOOL IsNvidiaGpuPresent(void) {
#ifdef HAS_NVAPI
    NvAPI_Status status = NvAPI_Initialize();
    if (status != NVAPI_OK)
        return FALSE;

    NvPhysicalGpuHandle gpuHandles[NVAPI_MAX_PHYSICAL_GPUS];
    NvU32 gpuCount = 0;
    status = NvAPI_EnumPhysicalGPUs(gpuHandles, &gpuCount);
    NvAPI_Unload();

    return (status == NVAPI_OK && gpuCount > 0);
#else
    return TRUE;
#endif
}

static void AddTrayIcon(void) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(IDR_MAIN_ICON));
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"Force Min Clock - NVIDIA Memory Clock Tool");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void RemoveTrayIcon(void) {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static void ShowContextMenu(void) {
    POINT pt;
    GetCursorPos(&pt);

    g_hMenu = CreatePopupMenu();
    AppendMenuW(g_hMenu, MF_STRING, ID_TRAY_APPLY, L"Apply Clock Settings");
    AppendMenuW(g_hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(g_hMenu, MF_STRING, ID_TRAY_MIN_DEFAULT, L"Reset to Defaults");
    AppendMenuW(g_hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(g_hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(g_hWnd);
    TrackPopupMenu(g_hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hWnd, NULL);
    DestroyMenu(g_hMenu);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_EXIT:
            RemoveTrayIcon();
            PostQuitMessage(0);
            break;
        case ID_TRAY_APPLY:
            ShowBubble(L"Clock settings applied.", L"Force Min Clock");
            break;
        case ID_TRAY_MIN_DEFAULT:
            g_clocks.minClock = 0;
            g_clocks.maxClock = 0;
            ShowBubble(L"Reset to default clocks.", L"Force Min Clock");
            break;
        }
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_hInstance = hInstance;

    if (!IsNvidiaGpuPresent()) {
        MessageBoxW(NULL,
            L"No NVIDIA GPU detected.\n\nThis application requires an NVIDIA GPU to run.",
            L"Force Min Clock",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ForceMinClockTray";
    RegisterClassW(&wc);

    g_hWnd = CreateWindowExW(0, wc.lpszClassName, L"Force Min Clock",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    if (!g_hWnd) {
        return 1;
    }

    AddTrayIcon();

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
