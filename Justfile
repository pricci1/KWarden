set shell := ["bash", "-cu"]

container := "kde-dev"
root := "/var/home/otto/Projects/kwarden"
build-dir := root + "/build"
flatpak-build-dir := "/tmp/kwarden-flatpak-build"
flatpak-state-dir := "/tmp/kwarden-flatpak-state"

# List available recipes
_default:
    @just --list

# Configure the debug build in the KDE development container
configure:
    distrobox enter {{container}} -- bash -lc 'cmake -S {{root}} -B {{build-dir}} -G Ninja -DCMAKE_BUILD_TYPE=Debug'

# Build KWarden in the KDE development container
build: configure
    distrobox enter {{container}} -- bash -lc 'cmake --build {{build-dir}}'

# Run a quick headless smoke test
smoke: build
    distrobox enter {{container}} -- bash -lc 'QT_QPA_PLATFORM=offscreen {{build-dir}}/bin/kwarden --quit-after-ms 250'

# Launch KWarden normally from the KDE development container
run: build
    distrobox enter {{container}} -- bash -lc '{{build-dir}}/bin/kwarden'

# Build the Flatpak version
flatpak-build:
    flatpak-builder --force-clean --state-dir={{flatpak-state-dir}} {{flatpak-build-dir}} org.kwarden.KWarden.yml

# Build and smoke-run the Flatpak version
flatpak-smoke: flatpak-build
    flatpak-builder --run --env=QT_QPA_PLATFORM=offscreen {{flatpak-build-dir}} org.kwarden.KWarden.yml kwarden --quit-after-ms 250

# Build and run the Flatpak version
flatpak-run: flatpak-build
    flatpak-builder --run \
        --share=ipc --socket=wayland --socket=fallback-x11 --device=dri \
        --env=QT_QPA_PLATFORM=wayland \
        {{flatpak-build-dir}} org.kwarden.KWarden.yml kwarden

# Run development environment smoke tests
dev-smoke:
    ./kde-dev-smoke-test.sh

# Run visible GUI smoke tests for native and Flatpak KDE environments
gui-smoke:
    KDE_GUI_SMOKE_SECONDS=${KDE_GUI_SMOKE_SECONDS:-10} ./kde-dev-gui-smoke-test.sh both
