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
#define ID_TRAY_MIN_CLOCK_BASE 2000
#define ID_TRAY_MAX_CLOCK_BASE 3000

#define MAX_CLOCK_ENTRIES 128

static HINSTANCE g_hInstance;
static HWND g_hWnd;
static NOTIFYICONDATAW g_nid = {0};

typedef struct {
    int minClockKHz;
    int maxClockKHz;
    int active;
} ClockSettings;

static ClockSettings g_clocks = {0, 0, 0};

#ifdef HAS_NVAPI
static NvPhysicalGpuHandle g_gpu = NULL;
static int g_memClocks[MAX_CLOCK_ENTRIES];
static int g_memClockCount = 0;
static NvAPI_ShortString g_gpuName = {0};
#endif

static int CompareInts(const void *a, const void *b) {
    return (*(const int *)a - *(const int *)b);
}

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

static void CollectMemoryClocks(void) {
#ifdef HAS_NVAPI
    NvU32 count = 0;
    NvPhysicalGpuHandle gpus[NVAPI_MAX_PHYSICAL_GPUS];
    if (NvAPI_EnumPhysicalGPUs(gpus, &count) != NVAPI_OK || count == 0)
        return;

    g_gpu = gpus[0];
    NvAPI_GPU_GetFullName(g_gpu, g_gpuName);

    NV_GPU_PERF_PSTATES20_INFO pstates = {0};
    pstates.version = NV_GPU_PERF_PSTATES20_INFO_VER;
    if (NvAPI_GPU_GetPstates20(g_gpu, &pstates) != NVAPI_OK)
        return;

    int found = 0;
    for (NvU32 p = 0; p < pstates.numPstates && found < MAX_CLOCK_ENTRIES; p++) {
        NV_GPU_PERF_PSTATES20_INFO *ps = &pstates;
        for (NvU32 c = 0; c < ps->numClocks && found < MAX_CLOCK_ENTRIES; c++) {
            NV_GPU_PSTATE20_CLOCK_ENTRY_V1 *clk = &ps->pstates[p].clocks[c];
            if (clk->domainId != NVAPI_GPU_PUBLIC_CLOCK_MEMORY)
                continue;

            if (clk->typeId == NVAPI_GPU_PERF_PSTATE20_CLOCK_TYPE_SINGLE) {
                int freq = (int)clk->data.single.freq_kHz;
                if (freq > 0) {
                    g_memClocks[found++] = freq;
                }
            } else if (clk->typeId == NVAPI_GPU_PERF_PSTATE20_CLOCK_TYPE_RANGE) {
                int minF = (int)clk->data.range.minFreq_kHz;
                int maxF = (int)clk->data.range.maxFreq_kHz;
                if (minF > 0) g_memClocks[found++] = minF;
                if (maxF > 0 && maxF != minF) g_memClocks[found++] = maxF;
            }
        }
    }

    g_memClockCount = found;
    if (g_memClockCount > 1) {
        qsort(g_memClocks, g_memClockCount, sizeof(int), CompareInts);
        int unique = 1;
        for (int i = 1; i < g_memClockCount; i++) {
            if (g_memClocks[i] != g_memClocks[unique - 1]) {
                g_memClocks[unique++] = g_memClocks[i];
            }
        }
        g_memClockCount = unique;
    }
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

static void BuildClockSubmenu(HMENU hParent, BOOL isMin) {
    HMENU hSub = CreatePopupMenu();
    int base = isMin ? ID_TRAY_MIN_CLOCK_BASE : ID_TRAY_MAX_CLOCK_BASE;
    int selected = isMin ? g_clocks.minClockKHz : g_clocks.maxClockKHz;

#ifdef HAS_NVAPI
    if (g_memClockCount == 0) {
        AppendMenuW(hSub, MF_STRING | MF_GRAYED, 0, L"No clock data available");
    } else {
        for (int i = 0; i < g_memClockCount; i++) {
            int freqKHz = g_memClocks[i];
            int freqMHz = freqKHz / 1000;
            wchar_t label[64];
            StringCchPrintfW(label, ARRAYSIZE(label), L"%d MHz", freqMHz);
            UINT flags = MF_STRING;
            if (freqKHz == selected)
                flags |= MF_CHECKED;
            AppendMenuW(hSub, flags, base + i, label);
        }
    }
#else
    AppendMenuW(hSub, MF_STRING | MF_GRAYED, 0, L"NVAPI not available");
#endif

    const wchar_t *title = isMin ? L"Min Memory Clock" : L"Max Memory Clock";
    AppendMenuW(hParent, MF_POPUP, (UINT_PTR)hSub, title);
}

static void ShowContextMenu(void) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();

    BuildClockSubmenu(hMenu, TRUE);
    BuildClockSubmenu(hMenu, FALSE);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_APPLY, L"Apply Clock Settings");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_MIN_DEFAULT, L"Reset to Defaults");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(g_hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hWnd, NULL);
    DestroyMenu(hMenu);
}

static void HandleClockSelection(WORD id, BOOL isMin) {
#ifdef HAS_NVAPI
    int idx = (int)(isMin ? (id - ID_TRAY_MIN_CLOCK_BASE) : (id - ID_TRAY_MAX_CLOCK_BASE));
    if (idx >= 0 && idx < g_memClockCount) {
        int freqMHz = g_memClocks[idx] / 1000;
        wchar_t msg[128];
        if (isMin) {
            g_clocks.minClockKHz = g_memClocks[idx];
            StringCchPrintfW(msg, ARRAYSIZE(msg), L"Min clock set to %d MHz.", freqMHz);
        } else {
            g_clocks.maxClockKHz = g_memClocks[idx];
            StringCchPrintfW(msg, ARRAYSIZE(msg), L"Max clock set to %d MHz.", freqMHz);
        }
        ShowBubble(msg, L"Force Min Clock");
    }
#endif
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
            ShowContextMenu();
        }
        return 0;

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id >= ID_TRAY_MIN_CLOCK_BASE && id < ID_TRAY_MIN_CLOCK_BASE + MAX_CLOCK_ENTRIES) {
            HandleClockSelection(id, TRUE);
        } else if (id >= ID_TRAY_MAX_CLOCK_BASE && id < ID_TRAY_MAX_CLOCK_BASE + MAX_CLOCK_ENTRIES) {
            HandleClockSelection(id, FALSE);
        } else {
            switch (id) {
            case ID_TRAY_EXIT:
                RemoveTrayIcon();
                PostQuitMessage(0);
                break;
            case ID_TRAY_APPLY:
                if (g_clocks.minClockKHz > 0 || g_clocks.maxClockKHz > 0) {
                    wchar_t msg[256];
                    StringCchPrintfW(msg, ARRAYSIZE(msg),
                        L"Min: %d MHz\nMax: %d MHz\n\n(Clock application pending NVAPI set implementation)",
                        g_clocks.minClockKHz / 1000,
                        g_clocks.maxClockKHz / 1000);
                    ShowBubble(msg, L"Force Min Clock");
                } else {
                    ShowBubble(L"No clock speeds selected.", L"Force Min Clock");
                }
                break;
            case ID_TRAY_MIN_DEFAULT:
                g_clocks.minClockKHz = 0;
                g_clocks.maxClockKHz = 0;
                ShowBubble(L"Reset to default clocks.", L"Force Min Clock");
                break;
            }
        }
        return 0;
    }

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

#ifdef HAS_NVAPI
    if (NvAPI_Initialize() != NVAPI_OK) {
        MessageBoxW(NULL,
            L"Failed to initialize NVAPI.\n\nPlease ensure NVIDIA drivers are installed.",
            L"Force Min Clock", MB_OK | MB_ICONERROR);
        return 1;
    }

    CollectMemoryClocks();

    if (g_gpu == NULL) {
        MessageBoxW(NULL,
            L"No NVIDIA GPU detected.\n\nThis application requires an NVIDIA GPU to run.",
            L"Force Min Clock", MB_OK | MB_ICONERROR);
        NvAPI_Unload();
        return 1;
    }
#else
    MessageBoxW(NULL,
        L"Built without NVAPI. Running in stub mode.",
        L"Force Min Clock", MB_OK | MB_ICONINFORMATION);
#endif

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ForceMinClockTray";
    RegisterClassW(&wc);

    g_hWnd = CreateWindowExW(0, wc.lpszClassName, L"Force Min Clock",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    if (!g_hWnd) {
#ifdef HAS_NVAPI
        NvAPI_Unload();
#endif
        return 1;
    }

    AddTrayIcon();

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

#ifdef HAS_NVAPI
    NvAPI_Unload();
#endif

    return (int)msg.wParam;
}
