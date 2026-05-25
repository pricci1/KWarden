# KWarden

KWarden is a KDE native GUI frontend for Bitwarden's official `bw` CLI.

The current development build uses mocked vault data that mirrors the official CLI response shape from `bitwarden/clients`:

- `bw list items` style wrapper: `{ "object": "list", "data": [...] }`
- login cipher items: `{ "object": "item", "type": 1, "login": { ... }, "fields": [...] }`

This keeps the UI useful while the real CLI provider is being built.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/kwarden
```

If you are using the KDE development container from this workspace:

```sh
distrobox enter kde-dev -- bash -lc 'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ./build/kwarden'
```

## Current features

- Native Qt Widgets/KDE Frameworks application
- Searchable vault item list
- Detail pane for username, password, TOTP, custom fields, and notes
- Clipboard copy buttons for secret values
- Mock data based on Bitwarden CLI item/list responses
