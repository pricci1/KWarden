cmake_minimum_required(VERSION 3.20)
project(kwarden VERSION 0.1.0 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n WidgetsAddons)

add_executable(kwarden
    src/main.cpp
)

target_compile_features(kwarden PRIVATE cxx_std_20)
target_link_libraries(kwarden PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    KF6::CoreAddons
    KF6::I18n
    KF6::WidgetsAddons
)

install(TARGETS kwarden ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
