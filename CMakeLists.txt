cmake_minimum_required(VERSION 3.20)
project(kwarden VERSION 0.3.1 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n Kirigami)

include(CTest)

set(KWARDEN_APP_ID cl.tri.kwarden)

if (COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(kwarden
    src/main.cpp
    src/totp.cpp
)

qt_add_qml_module(kwarden
    URI org.kwarden
    VERSION 1.0
    QML_FILES
        src/qml/Main.qml
        src/qml/VaultListItemDelegate.qml
        src/qml/VaultFieldDelegate.qml
    IMPORTS
        org.kde.kirigamiaddons.formcard
)

target_compile_features(kwarden PRIVATE cxx_std_20)
target_include_directories(kwarden PRIVATE src)
target_link_libraries(kwarden PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::CoreAddons
    KF6::I18n
    KF6::Kirigami
)

if (BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    qt_add_executable(totp_test
        tests/totp_test.cpp
        src/totp.cpp
    )
    target_compile_features(totp_test PRIVATE cxx_std_20)
    target_include_directories(totp_test PRIVATE src)
    target_link_libraries(totp_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME totp_test COMMAND totp_test)
endif()

install(TARGETS kwarden ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES ${KWARDEN_APP_ID}.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${KWARDEN_APP_ID}.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES ${KWARDEN_APP_ID}.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
