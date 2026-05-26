# KWarden

KWarden is a KDE-native graphical frontend for Bitwarden's official `bw` CLI.
It is currently a Kirigami/QML desktop application backed by real `bw` process calls.

## Status

This project is in early development. The UI can load vault data through the official Bitwarden CLI.

KWarden expects `bw` to be installed and available in `PATH`. If it is not in `PATH`, set `KW_BW_BIN` to the `bw` executable path. In the Flatpak build, `KW_BW_BIN` runs through `flatpak-spawn --host`, so it can point at a host-installed `bw`. Login is still delegated to the CLI:

- Run `bw login` in a terminal if the CLI is unauthenticated.
- KWarden checks `bw status --raw` on startup and refresh.
- If the vault is locked, KWarden unlocks with `bw unlock --raw` and then loads `bw list items`.

By default KWarden uses direct one-shot `bw` process calls. To try the experimental managed `bw serve` backend, launch with:

```sh
KWARDEN_BW_BACKEND=serve ./build/bin/kwarden
```

The `serve` backend starts `bw serve` bound to `localhost` on a random local port, calls the local REST API for status/unlock/list/lock, and stops the child server when KWarden drops the active session or exits.

## Security model

- The Bitwarden master password is sent to `bw unlock --raw` through stdin, not as a command-line argument.
- The returned `BW_SESSION` value is kept only in KWarden process memory.
- When `KWARDEN_BW_BACKEND=serve` is enabled, KWarden also keeps the managed `bw serve` process local to the app lifetime and binds it only to `localhost`.
- Clipboard copies are marked with KDE's `x-kde-passwordManagerHint=secret` hint so Plasma's clipboard history can skip them, and KWarden clears the clipboard after 45 seconds if the copied value is still present.
- Optional PIN unlock wraps the current `BW_SESSION` in memory only and is cleared when KWarden quits. PINs must be at least 6 characters, or 8 digits for numeric-only PINs. KWarden can persist the id of a selected vault item and, after a future master-password unlock, use that item’s password to recreate the in-memory PIN wrapper automatically.
- When PIN unlock is enabled, Lock is a KWarden-local soft lock: it clears loaded items and the active session, but keeps the in-memory PIN-wrapped session so the PIN can unlock again. Without PIN unlock, Lock runs `bw lock`.
- KWarden does not write the master password, PIN/password value, or session key to KDE Wallet, settings, logs, or disk. Only the configured PIN source item id is stored in the app’s settings.
- Quitting the app drops KWarden's in-memory session key and any PIN-wrapped session.

## Features

- Native Kirigami/QML KDE interface
- Searchable vault item list
- Detail pane for username, password, generated TOTP codes, custom fields, and notes
- Clipboard copy buttons for secret values
- Refresh, unlock, lock, and PIN controls for `bw` vault state
- KDE Plasma tray integration: closing the window hides KWarden to the tray; use the tray menu's Quit action to exit
- Keyboard shortcuts:
  - `Ctrl+U`: copy username
  - `Ctrl+P`: copy password
  - `Ctrl+T`: copy the current TOTP code without visual formatting spaces

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
flatpak-builder --force-clean --state-dir=/tmp/kwarden-flatpak-state /tmp/kwarden-flatpak-build cl.tri.kwarden.yml
flatpak-builder --run --env=QT_QPA_PLATFORM=offscreen /tmp/kwarden-flatpak-build cl.tri.kwarden.yml kwarden --quit-after-ms 250
```

## Flatpak releases

Tagged releases named `vX.Y.Z` publish a Flatpak repo to GitHub Pages for automatic updates. After the first release, install the remote from:

```sh
flatpak remote-add --if-not-exists kwarden https://OWNER.github.io/REPOSITORY/cl.tri.kwarden.flatpakrepo
flatpak install kwarden cl.tri.kwarden
```

For manual visual checks, launch the app normally. If capturing screenshots on KDE Wayland, use a delayed active-window Spectacle capture and close the test window afterward.

## Project layout

```text
CMakeLists.txt       Build definition for the Kirigami app
cl.tri.kwarden.yml
                     Flatpak Builder manifest for the app
src/main.cpp         Application bootstrap, clipboard bridge, and bw CLI provider
src/qml/Main.qml     Kirigami UI, search, unlock/lock state, copy actions
```

## Next steps

- Add focused tests for parsing `bw` JSON responses and copy behavior.
