#!/usr/bin/env bash
set -euo pipefail

container_name="${KDE_DEV_CONTAINER:-kde-dev}"
kde_sdk_branch="${KDE_SDK_BRANCH:-6.10}"

log() {
  printf '\n==> %s\n' "$*"
}

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'error: required command not found: %s\n' "$1" >&2
    exit 1
  fi
}

write_probe_project() {
  local project_dir="$1"

  cat >"${project_dir}/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.20)
project(kde_dev_smoke_probe VERSION 0.1 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n Config WidgetsAddons)

add_executable(kde-dev-smoke-probe main.cpp)
target_compile_features(kde-dev-smoke-probe PRIVATE cxx_std_20)
target_link_libraries(kde-dev-smoke-probe PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  KF6::CoreAddons
  KF6::I18n
  KF6::ConfigCore
  KF6::WidgetsAddons
)
CMAKE

  cat >"${project_dir}/main.cpp" <<'CPP'
#include <QApplication>
#include <QLabel>
#include <QString>
#include <KAboutData>
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("kde-dev-smoke-probe");
    KAboutData about(QStringLiteral("kde-dev-smoke-probe"),
                     i18n("KDE Dev Smoke Probe"),
                     QStringLiteral("0.1"),
                     i18n("Build and runtime probe for the KDE development environment"),
                     KAboutLicense::GPL_V3);
    KAboutData::setApplicationData(about);

    KConfig config(QStringLiteral("kde-dev-smoke-probe.conf"), KConfig::SimpleConfig);
    KConfigGroup group(&config, QStringLiteral("SmokeTest"));
    group.writeEntry("status", QStringLiteral("ok"));
    config.sync();

    QLabel label(i18n("KDE development environment smoke test passed"));
    label.resize(480, 80);

    return group.readEntry("status") == QStringLiteral("ok") ? 0 : 1;
}
CPP
}

run_native_container_probe() {
  local project_dir="$1"

  log "Native Distrobox probe (${container_name})"
  distrobox enter "$container_name" -- bash -lc "
    set -euo pipefail
    cmake -S '$project_dir' -B '$project_dir/build-native' -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build '$project_dir/build-native'
    QT_QPA_PLATFORM=offscreen '$project_dir/build-native/kde-dev-smoke-probe'
  "
}

run_flatpak_sdk_probe() {
  local project_dir="$1"

  log "Flatpak KDE SDK probe (org.kde.Sdk//${kde_sdk_branch})"
  flatpak run \
    --filesystem="$project_dir" \
    --env=QT_QPA_PLATFORM=offscreen \
    --command=bash \
    "org.kde.Sdk//${kde_sdk_branch}" \
    -lc "
      set -euo pipefail
      cmake -S '$project_dir' -B '$project_dir/build-flatpak-sdk' -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
      cmake --build '$project_dir/build-flatpak-sdk'
      '$project_dir/build-flatpak-sdk/kde-dev-smoke-probe'
    "
}

main() {
  require_command distrobox
  require_command flatpak

  if ! distrobox list | awk -F'|' '{ gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2 }' | grep -Fxq "$container_name"; then
    printf 'error: Distrobox container not found: %s\n' "$container_name" >&2
    exit 1
  fi

  if ! flatpak info "org.kde.Sdk//${kde_sdk_branch}" >/dev/null 2>&1; then
    printf 'error: Flatpak runtime not found: org.kde.Sdk//%s\n' "$kde_sdk_branch" >&2
    exit 1
  fi

  local tmp_dir
  tmp_dir="$(mktemp -d -t kde-dev-smoke-test.XXXXXX)"
  trap "rm -rf '$tmp_dir'" EXIT

  write_probe_project "$tmp_dir"
  run_native_container_probe "$tmp_dir"
  run_flatpak_sdk_probe "$tmp_dir"

  log "Smoke test passed"
}

main "$@"
