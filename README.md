# KWarden

KWarden is a KDE-native graphical frontend for Bitwarden's official `bw` CLI.
It is currently a Kirigami/QML desktop application backed by development mock data, with the UI structured so the mock provider can later be replaced by real `bw` process calls.

## Status

This project is in early development. The UI is functional, but vault data is mocked.

The mock data mirrors response shapes from the official `bitwarden/clients` CLI implementation:

- `bw list items` style wrapper: `{ "object": "list", "data": [...] }`
- login cipher items: `{ "object": "item", "type": 1, "login": { ... }, "fields": [...] }`

This keeps the app usable while the real CLI provider is being built.

## Features

- Native Kirigami/QML KDE interface
- Searchable vault item list
- Detail pane for username, password, TOTP, custom fields, and notes
- Clipboard copy buttons for secret values
- Keyboard shortcuts:
  - `Ctrl+U`: copy username
  - `Ctrl+P`: copy password
  - `Ctrl+T`: copy TOTP without visual formatting spaces
- Development mock data based on Bitwarden CLI item/list responses

## Build

KWarden expects a KDE Frameworks 6 / Qt 6 development environment with Kirigami, CMake, and Ninja.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/kwarden
```

Using the `kde-dev` Distrobox container from this workspace:

```sh
distrobox enter kde-dev -- bash -lc 'cmake -S /var/home/otto/Projects/kwarden -B /var/home/otto/Projects/kwarden/build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build /var/home/otto/Projects/kwarden/build && /var/home/otto/Projects/kwarden/build/bin/kwarden'
```

## Verification

For a quick headless smoke run:

```sh
distrobox enter kde-dev -- bash -lc 'cmake --build /var/home/otto/Projects/kwarden/build && QT_QPA_PLATFORM=offscreen /var/home/otto/Projects/kwarden/build/bin/kwarden --quit-after-ms 250'
```

To build and smoke-run the Flatpak version using the KDE runtime:

```sh
flatpak-builder --force-clean --state-dir=/tmp/kwarden-flatpak-state /tmp/kwarden-flatpak-build org.kwarden.KWarden.yml
flatpak-builder --run --env=QT_QPA_PLATFORM=offscreen /tmp/kwarden-flatpak-build org.kwarden.KWarden.yml kwarden --quit-after-ms 250
```

For manual visual checks, launch the app normally. If capturing screenshots on KDE Wayland, use a delayed active-window Spectacle capture and close the test window afterward.

## Project layout

```text
CMakeLists.txt       Build definition for the Kirigami app
org.kwarden.KWarden.yml
                     Flatpak Builder manifest for the app
src/main.cpp         Application bootstrap and clipboard bridge for QML
src/qml/Main.qml     Kirigami UI, mock vault data, search, copy actions
```

## Next steps

- Move mock vault data behind a provider boundary.
- Add a real `bw` CLI provider using `QProcess`.
- Handle locked/logged-out CLI states.
- Add focused tests for parsing `bw` JSON responses and copy behavior.
