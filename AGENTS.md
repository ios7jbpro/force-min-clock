# AGENTS.md

## Build

```bash
# From MSYS2 MinGW64 shell or with mingw64 on PATH:
cmake -G "MinGW Makefiles" -B build -S .
cmake --build build
```

Output: `build/force-min-clock.exe`

To kill the running app before rebuilding (elevated process):
```bash
sudo taskkill /F /IM force-min-clock.exe
```

## Rules

- **Commit every change.** Each logical change gets its own commit.
- **Update this file** on big changes that affect how agents should work with the codebase.

## Architecture

- `src/main.c` - Single-file Win32 app. Tray icon, nvidia-smi parsing, clock application.
- `src/resource.rc` - Icon resource and admin elevation manifest.
- `src/app.manifest` - Requests `requireAdministrator` so nvidia-smi runs elevated without per-action UAC.
- `icon.ico` - Tray icon (converted from original PNG).

## Key details

- App runs elevated from launch (manifest). No need for sudo when calling nvidia-smi.
- Clock enumeration: parses `nvidia-smi -q -d SUPPORTED_CLOCKS` output. Looks for `Memory` lines (indented).
- Clock application: `nvidia-smi -lmc <min>,<max>` via `cmd.exe /c` with temp file for output capture.
- Settings stored in registry `HKCU\Software\ForceMinClock` (MinClockMHz, MaxClockMHz, Active).
- If Active=1 on startup, clocks are re-applied automatically.
- nvidia-smi path is hardcoded to `C:\Windows\System32\nvidia-smi.exe` for elevated process compatibility.
