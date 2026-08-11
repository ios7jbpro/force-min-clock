#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include <stdio.h>
#include "resource.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_APPLY 1002
#define ID_TRAY_MIN_DEFAULT 1003
#define ID_TRAY_MAX_DEFAULT 1004
#define ID_TRAY_MIN_CLOCK_BASE 2000
#define ID_TRAY_MAX_CLOCK_BASE 3000

#define MAX_CLOCK_ENTRIES 128
#define SMI_PATH L"C:\\Windows\\System32\\nvidia-smi.exe"

static HINSTANCE g_hInstance;
static HWND g_hWnd;
static NOTIFYICONDATAW g_nid = {0};

typedef struct {
    int minClockMHz;
    int maxClockMHz;
} ClockSettings;

static ClockSettings g_clocks = {0, 0};
static int g_memClocks[MAX_CLOCK_ENTRIES];
static int g_memClockCount = 0;

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

static int RunSmiCommandA(const char *args, char *output, DWORD outputSize) {
    wchar_t cmdLine[512];
    wchar_t wArgs[256];
    wchar_t tmpFile[] = L"C:\\Windows\\Temp\\fmc_smi_out.txt";

    MultiByteToWideChar(CP_UTF8, 0, args, -1, wArgs, ARRAYSIZE(wArgs));
    StringCchPrintfW(cmdLine, ARRAYSIZE(cmdLine), L"/c \"\"%s\" %s > \"%s\" 2>&1\"", SMI_PATH, wArgs, tmpFile);

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = cmdLine;
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (!ShellExecuteExW(&sei)) {
        output[0] = '\0';
        return -1;
    }

    WaitForSingleObject(sei.hProcess, 15000);
    DWORD exitCode = 0;
    GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess);

    HANDLE hFile = CreateFileW(tmpFile, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        output[0] = '\0';
        return -1;
    }

    DWORD total = 0, bytesRead;
    while (total < outputSize - 1 &&
           ReadFile(hFile, output + total, outputSize - total - 1, &bytesRead, NULL) && bytesRead > 0) {
        total += bytesRead;
    }
    output[total] = '\0';
    CloseHandle(hFile);

    DeleteFileW(tmpFile);
    return (int)exitCode;
}

static int RunSmiElevated(const wchar_t *args) {
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = SMI_PATH;
    sei.lpParameters = args;
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (!ShellExecuteExW(&sei))
        return 1;

    WaitForSingleObject(sei.hProcess, 10000);
    DWORD exitCode = 0;
    GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess);
    return (int)exitCode;
}

static void CollectMemoryClocks(void) {
    char output[16384];
    int rc = RunSmiCommandA("-q -d SUPPORTED_CLOCKS", output, sizeof(output));

    if (rc < 0)
        return;

    int found = 0;
    char *p = output;
    while (*p && found < MAX_CLOCK_ENTRIES) {
        char *eol = strstr(p, "\n");
        int len = eol ? (int)(eol - p) : (int)strlen(p);

        if (len >= 6 && strncmp(p, "Memory", 6) == 0) {
            char *colon = memchr(p, ':', len);
            if (colon) {
                int mhz = 0;
                if (sscanf(colon + 1, " %d MHz", &mhz) == 1 && mhz > 0) {
                    g_memClocks[found++] = mhz;
                }
            }
        }

        if (!eol) break;
        p = eol + 1;
        if (*p == '\r') p++;
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
    int selected = isMin ? g_clocks.minClockMHz : g_clocks.maxClockMHz;

    if (g_memClockCount == 0) {
        AppendMenuW(hSub, MF_STRING | MF_GRAYED, 0, L"No clock data available");
    } else {
        for (int i = 0; i < g_memClockCount; i++) {
            int mhz = g_memClocks[i];
            wchar_t label[64];
            StringCchPrintfW(label, ARRAYSIZE(label), L"%d MHz", mhz);
            UINT flags = MF_STRING;
            if (mhz == selected)
                flags |= MF_CHECKED;
            AppendMenuW(hSub, flags, base + i, label);
        }
    }

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
    int idx = (int)(isMin ? (id - ID_TRAY_MIN_CLOCK_BASE) : (id - ID_TRAY_MAX_CLOCK_BASE));
    if (idx >= 0 && idx < g_memClockCount) {
        int mhz = g_memClocks[idx];
        wchar_t msg[128];
        if (isMin) {
            g_clocks.minClockMHz = mhz;
            StringCchPrintfW(msg, ARRAYSIZE(msg), L"Min clock set to %d MHz.", mhz);
        } else {
            g_clocks.maxClockMHz = mhz;
            StringCchPrintfW(msg, ARRAYSIZE(msg), L"Max clock set to %d MHz.", mhz);
        }
        ShowBubble(msg, L"Force Min Clock");
    }
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
                if (g_clocks.minClockMHz > 0 || g_clocks.maxClockMHz > 0) {
                    wchar_t args[128];
                    StringCchPrintfW(args, ARRAYSIZE(args), L"-lmc %d,%d",
                        g_clocks.minClockMHz, g_clocks.maxClockMHz);
                    int rc = RunSmiElevated(args);
                    if (rc == 0) {
                        wchar_t msg[128];
                        StringCchPrintfW(msg, ARRAYSIZE(msg),
                            L"Memory clocks locked:\nMin: %d MHz\nMax: %d MHz",
                            g_clocks.minClockMHz, g_clocks.maxClockMHz);
                        ShowBubble(msg, L"Force Min Clock");
                    } else {
                        ShowBubble(L"Failed to apply clock settings.", L"Force Min Clock");
                    }
                } else {
                    ShowBubble(L"No clock speeds selected.", L"Force Min Clock");
                }
                break;
            case ID_TRAY_MIN_DEFAULT:
                g_clocks.minClockMHz = 0;
                g_clocks.maxClockMHz = 0;
                RunSmiElevated(L"--reset-gpu-clocks");
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

    CollectMemoryClocks();

    if (g_memClockCount == 0) {
        wchar_t dbg[1024];
        wchar_t wArgs[] = L"-q -d SUPPORTED_CLOCKS";
        wchar_t cmdLine[512];
        wchar_t tmpFile[] = L"C:\\Windows\\Temp\\fmc_smi_out.txt";
        StringCchPrintfW(cmdLine, ARRAYSIZE(cmdLine),
            L"/c \"\"%s\" %s > \"%s\" 2>&1\"", SMI_PATH, wArgs, tmpFile);

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"open";
        sei.lpFile = L"cmd.exe";
        sei.lpParameters = cmdLine;
        sei.nShow = SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;

        BOOL execOk = ShellExecuteExW(&sei);
        DWORD exitCode = 999;
        if (execOk) {
            WaitForSingleObject(sei.hProcess, 15000);
            GetExitCodeProcess(sei.hProcess, &exitCode);
            CloseHandle(sei.hProcess);
        }

        BOOL fileOk = FALSE;
        char fileBuf[4096] = "";
        DWORD fileSize = 0;
        HANDLE hFile = CreateFileW(tmpFile, GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            ReadFile(hFile, fileBuf, sizeof(fileBuf) - 1, &fileSize, NULL);
            fileBuf[fileSize] = '\0';
            CloseHandle(hFile);
            fileOk = TRUE;
        }

        wchar_t fileContentW[4096] = L"";
        if (fileSize > 0) {
            MultiByteToWideChar(CP_UTF8, 0, fileBuf, -1, fileContentW, ARRAYSIZE(fileContentW));
        }

        StringCchPrintfW(dbg, ARRAYSIZE(dbg),
            L"ShellExecuteEx: %s\n"
            L"Exit code: %d\n"
            L"File exists: %s\n"
            L"File size: %d\n\n"
            L"Command:\n%s\n\n"
            L"Output (first 500 chars):\n%.500s",
            execOk ? L"OK" : L"FAILED",
            (int)exitCode,
            fileOk ? L"YES" : L"NO",
            (int)fileSize,
            cmdLine,
            fileContentW);

        MessageBoxW(NULL, dbg, L"Force Min Clock - Debug", MB_OK | MB_ICONERROR);
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
