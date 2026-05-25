#!/usr/bin/env bash
set -euo pipefail

container_name="${KDE_DEV_CONTAINER:-kde-dev}"
kde_sdk_branch="${KDE_SDK_BRANCH:-6.10}"
duration_seconds="${KDE_GUI_SMOKE_SECONDS:-10}"
target="${1:-native}"

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
project(kde_dev_gui_smoke_probe VERSION 0.1 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n WidgetsAddons)

add_executable(kde-dev-gui-smoke-probe main.cpp)
target_compile_features(kde-dev-gui-smoke-probe PRIVATE cxx_std_20)
target_link_libraries(kde-dev-gui-smoke-probe PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  KF6::CoreAddons
  KF6::I18n
  KF6::WidgetsAddons
)
CMAKE

  cat >"${project_dir}/main.cpp" <<'CPP'
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <KAboutData>
#include <KLocalizedString>
#include <KMessageWidget>

static int closeDelayMs(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--seconds")) {
            bool ok = false;
            const int seconds = QString::fromLocal8Bit(argv[i + 1]).toInt(&ok);
            if (ok && seconds > 0) {
                return seconds * 1000;
            }
        }
    }

    return 10000;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("kde-dev-gui-smoke-probe");
    KAboutData about(QStringLiteral("kde-dev-gui-smoke-probe"),
                     i18n("KDE GUI Smoke Probe"),
                     QStringLiteral("0.1"),
                     i18n("Visible hello-world kitchen-sink probe for KDE development"),
                     KAboutLicense::GPL_V3);
    KAboutData::setApplicationData(about);

    QMainWindow window;
    window.setWindowTitle(i18n("Hello KDE — GUI Smoke Test"));
    window.resize(820, 560);

    auto *central = new QWidget(&window);
    auto *root = new QVBoxLayout(central);

    auto *hello = new QLabel(i18n("Hello, world from a native KDE/Qt app!"), central);
    QFont helloFont = hello->font();
    helloFont.setPointSize(22);
    helloFont.setBold(true);
    hello->setFont(helloFont);
    hello->setAlignment(Qt::AlignCenter);
    root->addWidget(hello);

    auto *message = new KMessageWidget(i18n("If you can see this window, the KDE development environment can build and run GUI apps."), central);
    message->setMessageType(KMessageWidget::Positive);
    message->setCloseButtonVisible(false);
    root->addWidget(message);

    auto *tabs = new QTabWidget(central);

    auto *controlsTab = new QWidget(tabs);
    auto *controlsLayout = new QVBoxLayout(controlsTab);
    auto *formBox = new QGroupBox(i18n("Controls"), controlsTab);
    auto *form = new QFormLayout(formBox);
    auto *nameEdit = new QLineEdit(i18n("KDE developer"), formBox);
    auto *combo = new QComboBox(formBox);
    combo->addItems({i18n("Qt Widgets"), i18n("Kirigami/QML"), i18n("KParts"), i18n("Plasma")});
    auto *spin = new QSpinBox(formBox);
    spin->setRange(1, 99);
    spin->setValue(43);
    auto *check = new QCheckBox(i18n("Wayland session detected"), formBox);
    check->setChecked(qEnvironmentVariableIsSet("WAYLAND_DISPLAY"));
    form->addRow(i18n("Name:"), nameEdit);
    form->addRow(i18n("Stack:"), combo);
    form->addRow(i18n("Aurora/Fedora version:"), spin);
    form->addRow(QString(), check);
    controlsLayout->addWidget(formBox);

    auto *progress = new QProgressBar(controlsTab);
    progress->setRange(0, 100);
    progress->setValue(72);
    progress->setFormat(i18n("Smoke test confidence: %p%"));
    controlsLayout->addWidget(progress);

    auto *slider = new QSlider(Qt::Horizontal, controlsTab);
    slider->setRange(0, 100);
    slider->setValue(progress->value());
    QObject::connect(slider, &QSlider::valueChanged, progress, &QProgressBar::setValue);
    controlsLayout->addWidget(slider);

    auto *buttonRow = new QHBoxLayout;
    auto *helloButton = new QPushButton(i18n("Say hello"), controlsTab);
    auto *closeButton = new QPushButton(i18n("Close"), controlsTab);
    buttonRow->addWidget(helloButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);
    controlsLayout->addLayout(buttonRow);
    tabs->addTab(controlsTab, i18n("Controls"));

    auto *dataTab = new QWidget(tabs);
    auto *dataLayout = new QHBoxLayout(dataTab);
    auto *list = new QListWidget(dataTab);
    list->addItems({i18n("CMake"), i18n("Ninja"), i18n("Qt 6"), i18n("KDE Frameworks 6"), i18n("Distrobox"), i18n("Flatpak SDK")});
    auto *notes = new QTextEdit(dataTab);
    notes->setPlainText(i18n("Kitchen sink:\n• label\n• KDE message widget\n• form controls\n• list\n• text edit\n• menu/status bar\n• auto-close timer"));
    dataLayout->addWidget(list, 1);
    dataLayout->addWidget(notes, 2);
    tabs->addTab(dataTab, i18n("Kitchen sink"));

    root->addWidget(tabs);
    window.setCentralWidget(central);

    auto *fileMenu = window.menuBar()->addMenu(i18n("File"));
    auto *closeAction = fileMenu->addAction(i18n("Close"));
    QObject::connect(closeAction, &QAction::triggered, &window, &QWidget::close);
    QObject::connect(closeButton, &QPushButton::clicked, &window, &QWidget::close);
    QObject::connect(helloButton, &QPushButton::clicked, [&]() {
        window.statusBar()->showMessage(i18n("Hello at %1", QDateTime::currentDateTime().toString(Qt::ISODate)), 3000);
    });

    const int delayMs = closeDelayMs(argc, argv);
    window.statusBar()->showMessage(i18n("Auto-closing in %1 seconds", delayMs / 1000));
    QTimer::singleShot(delayMs, &window, &QWidget::close);

    window.show();
    return app.exec();
}
CPP
}

run_native_probe() {
  local project_dir="$1"

  log "Native visible GUI probe (${container_name})"
  distrobox enter "$container_name" -- bash -lc "
    set -euo pipefail
    cmake -S '$project_dir' -B '$project_dir/build-native' -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build '$project_dir/build-native'
    '$project_dir/build-native/kde-dev-gui-smoke-probe' --seconds '$duration_seconds'
  "
}

run_flatpak_sdk_probe() {
  local project_dir="$1"

  log "Flatpak KDE SDK visible GUI probe (org.kde.Sdk//${kde_sdk_branch})"
  flatpak run \
    --filesystem="$project_dir" \
    --share=ipc \
    --socket=wayland \
    --socket=fallback-x11 \
    --device=dri \
    --command=bash \
    "org.kde.Sdk//${kde_sdk_branch}" \
    -lc "
      set -euo pipefail
      cmake -S '$project_dir' -B '$project_dir/build-flatpak-sdk' -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
      cmake --build '$project_dir/build-flatpak-sdk'
      '$project_dir/build-flatpak-sdk/kde-dev-gui-smoke-probe' --seconds '$duration_seconds'
    "
}

main() {
  require_command distrobox
  require_command flatpak

  case "$target" in
    native|flatpak|both) ;;
    *)
      printf 'usage: %s [native|flatpak|both]\n' "$0" >&2
      exit 2
      ;;
  esac

  if [[ "$target" == "native" || "$target" == "both" ]]; then
    if ! distrobox list | awk -F'|' '{ gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2 }' | grep -Fxq "$container_name"; then
      printf 'error: Distrobox container not found: %s\n' "$container_name" >&2
      exit 1
    fi
  fi

  if [[ "$target" == "flatpak" || "$target" == "both" ]]; then
    if ! flatpak info "org.kde.Sdk//${kde_sdk_branch}" >/dev/null 2>&1; then
      printf 'error: Flatpak runtime not found: org.kde.Sdk//%s\n' "$kde_sdk_branch" >&2
      exit 1
    fi
  fi

  local tmp_dir
  tmp_dir="$(mktemp -d -t kde-dev-gui-smoke-test.XXXXXX)"
  trap "rm -rf '$tmp_dir'" EXIT

  write_probe_project "$tmp_dir"

  if [[ "$target" == "native" || "$target" == "both" ]]; then
    run_native_probe "$tmp_dir"
  fi

  if [[ "$target" == "flatpak" || "$target" == "both" ]]; then
    run_flatpak_sdk_probe "$tmp_dir"
  fi

  log "Visible GUI smoke test passed"
}

main "$@"
