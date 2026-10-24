# Application Geometry Recovery

Application Geometry Recovery saves the positions and sizes of all open windows
in named profiles and restores them on request. A typical use is a computer
that is used both locally with several monitors and via Remote Desktop: each
monitor configuration gets its own profile, and one click puts every window
back where it belongs.

The application runs in the Windows system tray. Its user interface is in
English and German (depending on the Windows display language).

## Features

- Profiles with the geometry of all open top-level windows (position, size,
  normal/minimized/maximized state, monitor)
- Several instances of the same application are told apart and restored in
  order
- Optional start of applications that are not running when a profile is
  loaded (per window)
- Windows of applications that are started later are placed automatically
  according to the active profile, with an optional default geometry for
  additional windows
- Update check against the GitHub releases of this repository
- Profiles are stored in one XML file:
  `%APPDATA%\LightningSoftware\ApplicationGeometryRecovery\profiles.xml`

## Installation

1. Download `ApplicationGeometryRecovery-<version>-win64.exe` from the
   [latest release](https://github.com/Halikanese/application-geomertry-recovery/releases/latest).
2. Run the setup program and follow the wizard:
   1. **Installation folder**: keep the default or choose another folder.
   2. **License agreement**: accept the MIT license.
   3. **Shortcuts**: choose whether a Start menu shortcut should be created
      and whether the application starts automatically when you log in to
      Windows (recommended, the application has to run in the tray to
      restore profiles).
   4. **Start menu folder** (only with a Start menu shortcut).
   5. Click **Install**.
3. Start *Application Geometry Recovery* from the Start menu (or log off and
   on again if you chose autostart). The icon with two overlapping windows
   appears in the system tray.

To uninstall, run `maintenancetool.exe` in the installation folder. Your
profiles in `%APPDATA%` are kept.

## Step by step

### 1. Arrange your windows

Open your applications and arrange their windows the way you want them for
the current monitor configuration, for example local with two monitors.

### 2. Save a profile

1. Click the tray icon (left or right click).
2. Choose **Save…**.
3. Enter a name for the profile, for example `Local`, or pick an existing
   profile from the list and confirm.

All visible application windows (the ones shown in the taskbar) are saved.
If you save under an existing name, the profile is updated: applications
that are running get their current geometry, saved windows of applications
that are not running are kept.

Repeat steps 1 and 2 for each monitor configuration, for example once while
connected via Remote Desktop under the name `Remote Desktop`.

### 3. Restore a profile

1. Click the tray icon.
2. Open **Load** and click the profile.

The windows are moved back to their saved position, size and state. A
notification shows how many windows were restored and which ones were not
found. The loaded profile becomes the *active profile* (checked in the
**Load** menu).

### 4. Review and adjust a profile

Choose **Open** in the tray menu to open the main window:

- The left list shows all profiles. Select one to see its content.
- The right list shows the saved applications; expand an application to see
  its windows (instances) with state, geometry and monitor.
- **Default geometry**: if an application opens more windows than the
  profile contains, new windows get the geometry of the checked window. At
  most one window per application can be checked.
- **Autostart**: when the profile is loaded and this window is not open, the
  application is started and its window is placed as soon as it appears.
- Right click a window or an application and choose **Delete entry** to
  remove it from the profile.
- **Profile → Restore** loads the selected profile, **Profile → Delete**
  removes it.

Changes in the main window are saved immediately.

### 5. Applications started later

While a profile is active, the application watches for new windows. When you
start an application that is part of the active profile, its window is
placed according to the profile: first on a saved window that is not yet
occupied, otherwise on the window marked as default geometry. Windows of
other applications are left alone.

### 6. Updates

Choose **Extra → Check for updates…** in the main window. If a newer release
is available, the setup program is downloaded, started, and the application
closes for the installation.

## Building from source

Requirements:

- Qt 6.12 with MinGW 64-bit (Widgets, Network, Linguist Tools)
- CMake 3.16 or newer and Ninja (both come with the Qt installer)
- Optional: Qt Installer Framework 4.x for the setup program
  (Qt Maintenance Tool → Developer and Designer Tools)

Build with Qt Creator by opening `CMakeLists.txt`, or on the command line:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
cmake --build build
```

Create the setup program (or a ZIP archive without the Installer Framework):

```bat
cmake --build build --target package
```

Translations are in `i18n/*.ts`. After changing texts in the code, run the
`update_translations` target and edit the German translation with Qt
Linguist.

The repository for the update check is set with the CMake variable
`AGR_UPDATE_REPOSITORY` (default: this repository). Release tags must match
the project version in `CMakeLists.txt`, for example `v0.1`.

## License

MIT, see [LICENSE](LICENSE).
