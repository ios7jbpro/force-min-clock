# Force Min Clock

A system tray tool for NVIDIA GPUs that forces minimum and maximum memory clock speeds.

## Features

- Tray icon with right-click context menu
- Enumerates all supported memory clock speeds from your GPU
- Lock min/max memory clocks to any supported value
- Reset to default clocks
- Settings persist across restarts via registry
- Runs elevated automatically (UAC manifest) - no repeated prompts

## Requirements

- Windows 10/11
- NVIDIA GPU with up-to-date drivers
- `nvidia-smi.exe` must be present (ships with NVIDIA drivers at `C:\Windows\System32\nvidia-smi.exe`)

## Usage

1. Run `force-min-clock.exe` (will request admin elevation on launch)
2. Right-click the tray icon
3. Select a **Min Memory Clock** and **Max Memory Clock** from the submenus
4. Click **Apply Clock Settings** to lock the clocks
5. Click **Reset to Defaults** to restore default memory clocks

If you applied clocks in a previous session, they will be re-applied automatically on next startup.

The app also registers itself in Windows startup (`HKCU\...\Run`) so it launches automatically on login. This can be toggled from the tray menu via **Autostart with Windows** (enabled by default).

## Building

Requires [MSYS2](https://www.msys2.org/) with MinGW-w64 toolchain:

```bash
# In MSYS2 MinGW64 shell:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-toolchain

# Build:
cmake -G "MinGW Makefiles" -B build -S .
cmake --build build
```

Output: `build/force-min-clock.exe`

## How it works

- Queries supported clocks via `nvidia-smi -q -d SUPPORTED_CLOCKS`
- Locks clocks via `nvidia-smi -lmc <min>,<max>`
- Resets via `nvidia-smi --reset-memory-clocks`
- Settings stored in registry at `HKCU\Software\ForceMinClock`
