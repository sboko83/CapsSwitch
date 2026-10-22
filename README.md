# CapsSwitch

[Русский](README.ru.md) | English

<img src="assets/CapsSwitch.png" alt="CapsSwitch icon" width="96">

Switch the Windows keyboard layout with **Caps Lock**, just like **Win + Space**.
CapsSwitch runs in the system tray, does not record what you type, and uses no network.
The interface is Russian when the Windows display language is Russian; otherwise English.

## Start

Requires Windows 10/11 and at least two keyboard layouts. No additional runtime is needed.

**Setup.** Download `CapsSwitch-…-windows-setup.exe` from **Releases** and run it.
By default it installs for the current user without administrator rights; it can
also install for all users into Program Files. Setup can start CapsSwitch at sign-in,
adds it to the Start menu, and removes it via **Settings → Apps**.

**Portable.** Download the **x64** or **ARM64** ZIP, extract it, and run `CapsSwitch.exe`.
For startup at sign-in, place a shortcut to the EXE in `shell:startup` (Win + R).
Remove that shortcut before moving or deleting the app.

## Controls

| Action | Result |
| --- | --- |
| Caps Lock | Next layout, once per press |
| Shift + Caps Lock | Normal Caps Lock |
| Caps Lock with Ctrl, Alt, or Win | Normal key behavior |
| Right-click the tray icon | Pause, restart as administrator, about, exit |
| Double-click the tray icon | Pause or resume |

While paused, Caps Lock works normally. Only one instance runs per Windows session.
Before updating or deleting the EXE, choose **Exit** or run `CapsSwitch.exe --stop`.
In windows running as administrator, Caps Lock switches layouts only when CapsSwitch runs
elevated too: right-click the tray icon and choose **Restart as administrator**. After a new
sign-in the app starts with normal rights again.
The sign-in screen and secure UAC desktop are not supported.

## Build

Requires PowerShell and Visual Studio Build Tools 2022 with the C++ workload,
Windows SDK, and ARM64 build tools for ARM64 output. The setup needs Inno Setup 6
(`winget install JRSoftware.InnoSetup`) and is built only with both architectures.

```powershell
.\build.ps1 build                 # bin/x64 and bin/arm64
.\build.ps1 test                  # x64 checks
.\build.ps1 package               # build, test, ZIPs, setup and SHA-256 in dist/
.\build.ps1 package -Arch x64      # x64 only
```

GitHub Actions runs the same packaging command. Tests cover keyboard state,
tray icons, and hook lifecycle; layout switching also needs a desktop check.
[Architecture](docs/architecture.md) (Russian).

## Acknowledgements

CapsSwitch was written with the assistance of Claude Code, Codex, and ZCode.

## License

[MIT](LICENSE).
