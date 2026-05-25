# KDE Development Smoke Tests

This workspace includes two smoke tests for the KDE native desktop development environment on Aurora/Kinoite without layering host RPM dependencies.

## Prerequisites

The setup expects:

- Distrobox container: `kde-dev`
- Flatpak KDE SDK: `org.kde.Sdk//6.10`
- Flatpak KDE Platform: `org.kde.Platform//6.10`
- Flatpak Builder installed

The scripts use temporary build directories under `/tmp` and clean them up automatically.

## Headless end-to-end smoke test

Script: [`kde-dev-smoke-test.sh`](./kde-dev-smoke-test.sh)

This test generates a temporary Qt/KDE Frameworks CMake project, builds it, and runs it headlessly with `QT_QPA_PLATFORM=offscreen`.

It verifies both paths:

- Native Distrobox build/run via `kde-dev`
- Flatpak SDK build/run via `org.kde.Sdk//6.10`

Run it with:

```sh
./kde-dev-smoke-test.sh
```

Expected final output:

```text
==> Smoke test passed
```

## Visible GUI kitchen-sink smoke test

Script: [`kde-dev-gui-smoke-test.sh`](./kde-dev-gui-smoke-test.sh)

This test generates a temporary KDE/Qt Widgets application, builds it, and launches a visible window. The window includes a small “hello world” kitchen sink:

- Large hello-world label
- KDE `KMessageWidget`
- Tabs
- Form controls
- Combo box
- Checkbox
- Spin box
- Slider and progress bar
- List widget
- Text edit
- Menu bar
- Status bar
- Auto-close timer

Run the native Distrobox GUI test:

```sh
./kde-dev-gui-smoke-test.sh native
```

Run the Flatpak KDE SDK GUI test:

```sh
./kde-dev-gui-smoke-test.sh flatpak
```

Run both GUI tests sequentially:

```sh
./kde-dev-gui-smoke-test.sh both
```

Expected final output:

```text
==> Visible GUI smoke test passed
```

## Configuration

Both scripts support environment variable overrides.

### `KDE_DEV_CONTAINER`

Default: `kde-dev`

Use a different Distrobox container name:

```sh
KDE_DEV_CONTAINER=my-kde-container ./kde-dev-smoke-test.sh
```

### `KDE_SDK_BRANCH`

Default: `6.10`

Use a different KDE Flatpak SDK branch:

```sh
KDE_SDK_BRANCH=6.9 ./kde-dev-smoke-test.sh
```

### `KDE_GUI_SMOKE_SECONDS`

Default: `10`

Controls how long the visible GUI smoke-test window stays open before auto-closing:

```sh
KDE_GUI_SMOKE_SECONDS=30 ./kde-dev-gui-smoke-test.sh native
```

## Recommended validation sequence

After changing the development environment, run:

```sh
./kde-dev-smoke-test.sh
KDE_GUI_SMOKE_SECONDS=10 ./kde-dev-gui-smoke-test.sh native
KDE_GUI_SMOKE_SECONDS=10 ./kde-dev-gui-smoke-test.sh flatpak
```

If all three commands pass, the native Distrobox path and Flatpak SDK path can both compile and run basic KDE/Qt applications.

## Notes

- The headless test is suitable for quick automated checks.
- The GUI test is meant for manual visual confirmation that an actual window can appear on the current KDE/Wayland session.
- The generated test projects are intentionally temporary; edit the scripts if you want to change the smoke-test application contents.
