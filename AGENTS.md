# AGENTS.md

## Project

KWarden is a KDE-native Kirigami/QML frontend for Bitwarden's official `bw` CLI. It currently uses mocked Bitwarden CLI-shaped data while the real CLI provider is being developed.

## Build and verification

- Preferred dev environment: `kde-dev` Distrobox container.
- Configure/build:
  ```sh
  distrobox enter kde-dev -- bash -lc 'cmake -S /var/home/otto/Projects/kwarden -B /var/home/otto/Projects/kwarden/build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build /var/home/otto/Projects/kwarden/build'
  ```
- Quick smoke run:
  ```sh
  distrobox enter kde-dev -- bash -lc 'QT_QPA_PLATFORM=offscreen /var/home/otto/Projects/kwarden/build/bin/kwarden --quit-after-ms 250'
  ```

## Coding guidance

- Keep the UI in Kirigami/QML unless explicitly asked otherwise.
- Keep C++ small and focused on application bootstrap, platform integration, and future provider/backend code.
- Mock data should continue to resemble official Bitwarden CLI JSON shapes (`object: "list"`, `object: "item"`, `login`, `fields`, etc.).
- TOTP may be visually grouped with spaces, but clipboard copies should remove spaces.
- Prefer small, focused changes and verify with the narrowest build/smoke check that gives confidence.

## Screenshots

When taking KDE Wayland screenshots, use delayed Spectacle active-window capture, ask the user to keep KWarden focused during the countdown, verify the file was written, and close test KWarden windows afterward.
