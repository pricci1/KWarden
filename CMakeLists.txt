cmake_minimum_required(VERSION 3.20)
project(kwarden VERSION 0.1.0 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n Kirigami)

if (COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(kwarden
    src/main.cpp
)

qt_add_qml_module(kwarden
    URI org.kwarden
    VERSION 1.0
    QML_FILES
        src/qml/Main.qml
)

target_compile_features(kwarden PRIVATE cxx_std_20)
target_link_libraries(kwarden PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::CoreAddons
    KF6::I18n
    KF6::Kirigami
)

install(TARGETS kwarden ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
