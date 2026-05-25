# KWarden

KWarden is a KDE-native graphical frontend for Bitwarden's official `bw` CLI.
It is currently a Kirigami/QML desktop application backed by real `bw` process calls.

## Status

This project is in early development. The UI can load vault data through the official Bitwarden CLI.

KWarden expects `bw` to be installed and available in `PATH`. Login is still delegated to the CLI:

- Run `bw login` in a terminal if the CLI is unauthenticated.
- KWarden checks `bw status --raw` on startup and refresh.
- If the vault is locked, KWarden unlocks with `bw unlock --raw` and then loads `bw list items`.

## Security model

- The Bitwarden master password is sent to `bw unlock --raw` through stdin, not as a command-line argument.
- The returned `BW_SESSION` value is kept only in KWarden process memory.
- Optional PIN unlock is ephemeral. It wraps the current `BW_SESSION` in memory only and is cleared when KWarden quits.
- When PIN unlock is enabled, Lock is a KWarden-local soft lock: it clears loaded items and the active session, but keeps the in-memory PIN-wrapped session so the PIN can unlock again. Without PIN unlock, Lock runs `bw lock`.
- KWarden does not write the master password, PIN, or session key to KDE Wallet, settings, logs, or disk.
- Quitting the app drops KWarden's in-memory session key and any PIN-wrapped session. Persistent PIN unlock is intentionally not implemented.

## Features

- Native Kirigami/QML KDE interface
- Searchable vault item list
- Detail pane for username, password, TOTP, custom fields, and notes
- Clipboard copy buttons for secret values
- Refresh, unlock, lock, and ephemeral PIN controls for `bw` vault state
- Keyboard shortcuts:
  - `Ctrl+U`: copy username
  - `Ctrl+P`: copy password
  - `Ctrl+T`: copy TOTP without visual formatting spaces

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
src/main.cpp         Application bootstrap, clipboard bridge, and bw CLI provider
src/qml/Main.qml     Kirigami UI, search, unlock/lock state, copy actions
```

## Next steps

- Add focused tests for parsing `bw` JSON responses and copy behavior.
