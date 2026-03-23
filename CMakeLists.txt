cmake_minimum_required(VERSION 3.10)
if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif ()
if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif ()

project(proxor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()

# WINDOWS PDB FILE
if (WIN32)
    if (MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    endif ()
endif ()

# Find Qt
set(PROXOR_LOCAL_QT_DIR "${CMAKE_SOURCE_DIR}/qtsdk/Qt")
if (EXISTS "${PROXOR_LOCAL_QT_DIR}/lib/cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${PROXOR_LOCAL_QT_DIR}")
endif ()

if (WIN32 AND MINGW AND EXISTS "${PROXOR_LOCAL_QT_DIR}/lib/cmake")
    message(FATAL_ERROR
        "The bundled Qt SDK at ${PROXOR_LOCAL_QT_DIR} is the Visual Studio runtime and cannot be linked with MinGW.\n"
        "Configure Proxor from a Visual Studio 2022 developer shell so CMake picks cl.exe,\n"
        "or override CMAKE_PREFIX_PATH/Qt6_DIR with a MinGW-compatible Qt SDK.")
endif ()

if (NOT QT_VERSION_MAJOR)
    if (DEFINED Qt5_DIR AND NOT DEFINED Qt6_DIR)
        set(QT_VERSION_MAJOR 5)
    else ()
        set(QT_VERSION_MAJOR 6)
    endif ()
endif ()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Svg)

if (NKR_CROSS)
    set_property(TARGET Qt${QT_VERSION_MAJOR}::moc PROPERTY IMPORTED_LOCATION /usr/bin/moc)
    set_property(TARGET Qt${QT_VERSION_MAJOR}::uic PROPERTY IMPORTED_LOCATION /usr/bin/uic)
    set_property(TARGET Qt${QT_VERSION_MAJOR}::rcc PROPERTY IMPORTED_LOCATION /usr/bin/rcc)
endif ()

#### Platform Variables ####
if (WIN32)
    include("cmake/windows/windows.cmake")
else ()
    include("cmake/linux/linux.cmake")
endif ()

#### default prefix path ####

if (NOT NKR_LIBS)
    if (NKR_PACKAGE)
        list(APPEND NKR_LIBS ${CMAKE_SOURCE_DIR}/libs/deps/package)
    else ()
        list(APPEND NKR_LIBS ${CMAKE_SOURCE_DIR}/libs/deps/built)
    endif ()
endif ()

if (NOT NKR_DISABLE_LIBS)
    list(APPEND CMAKE_PREFIX_PATH ${NKR_LIBS})
endif ()

message("[CMAKE_PREFIX_PATH] ${CMAKE_PREFIX_PATH}")

# for some cross toolchain
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_PREFIX_PATH})
message("[CMAKE_FIND_ROOT_PATH] ${CMAKE_FIND_ROOT_PATH}")

#### NKR ####

include("cmake/print.cmake")
include("cmake/nkr.cmake")

find_package(Threads)

#### NKR EXTERNAL ####

if (NKR_NO_EXTERNAL)
    set(NKR_NO_GRPC 1)
    set(NKR_NO_YAML 1)
    set(NKR_NO_ZXING 1)
    set(NKR_NO_QHOTKEY 1)
endif ()

# grpc
if (NKR_NO_GRPC)
    nkr_add_compile_definitions(NKR_NO_GRPC)
else ()
    # My proto
    include("cmake/myproto.cmake")
    list(APPEND NKR_EXTERNAL_TARGETS myproto)
endif ()

# yaml-cpp
if (NKR_NO_YAML)
    nkr_add_compile_definitions(NKR_NO_YAML)
else ()
    find_package(yaml-cpp CONFIG REQUIRED) # only Release is built
    list(APPEND NKR_EXTERNAL_TARGETS yaml-cpp)
endif ()

# zxing-cpp
if (NKR_NO_ZXING)
    nkr_add_compile_definitions(NKR_NO_ZXING)
else ()
    find_package(ZXing CONFIG REQUIRED)
    list(APPEND NKR_EXTERNAL_TARGETS ZXing::ZXing)
endif ()

# QHotkey (static submodule)
if (NKR_NO_QHOTKEY)
    nkr_add_compile_definitions(NKR_NO_QHOTKEY)
else ()
    set(QT_DEFAULT_MAJOR_VERSION ${QT_VERSION_MAJOR})
    set(QHOTKEY_INSTALL OFF)
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(3rdparty/QHotkey)
    list(APPEND NKR_EXTERNAL_TARGETS qhotkey)
endif ()

#### debug print ####

if (DBG_CMAKE)
    print_all_variables()
    print_target_properties(myproto)
    print_target_properties(yaml-cpp)
    print_target_properties(ZXing::ZXing)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")
endif ()

# Sources
set(PROJECT_SOURCES
        ${PLATFORM_SOURCES}

        main/main.cpp
        main/ProxorGui.cpp
        main/ProxorGui_Utils.cpp
        main/HTTPRequestHelper.cpp

        3rdparty/base64.cpp
        3rdparty/qrcodegen.cpp
        3rdparty/QtExtKeySequenceEdit.cpp

        3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.cpp
        3rdparty/qv2ray/v2/components/proxy/QvProxyConfigurator.cpp
        3rdparty/qv2ray/v2/ui/widgets/common/QJsonModel.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.ui

        3rdparty/qv2ray/v3/components/GeositeReader/GeositeReader.cpp
        3rdparty/qv2ray/v3/components/GeositeReader/picoproto.cpp

        rpc/gRPC.cpp

        db/Database.cpp
        db/traffic/TrafficLooper.cpp
        db/ProfileFilter.cpp
        db/ConfigBuilder.cpp

        fmt/AbstractBean.cpp
        fmt/Bean2CoreObj_box.cpp
        fmt/Bean2External.cpp
        fmt/Bean2Link.cpp
        fmt/Link2Bean.cpp
        fmt/ChainBean.hpp # translate

        sub/GroupUpdater.cpp

        sys/ExternalProcess.cpp
        sys/AutoRun.cpp

        ui/ThemeManager.cpp
        ui/Icon.cpp

        ui/mainwindow_grpc.cpp
        ui/mainwindow.cpp
        ui/mainwindow.h
        ui/mainwindow.ui

        ui/edit/dialog_edit_profile.h
        ui/edit/dialog_edit_profile.cpp
        ui/edit/dialog_edit_profile.ui
        ui/edit/dialog_edit_group.h
        ui/edit/dialog_edit_group.cpp
        ui/edit/dialog_edit_group.ui

        ui/edit/edit_chain.h
        ui/edit/edit_chain.cpp
        ui/edit/edit_chain.ui
        ui/edit/edit_socks_http.h
        ui/edit/edit_socks_http.cpp
        ui/edit/edit_socks_http.ui
        ui/edit/edit_shadowsocks.h
        ui/edit/edit_shadowsocks.cpp
        ui/edit/edit_shadowsocks.ui
        ui/edit/edit_vmess.h
        ui/edit/edit_vmess.cpp
        ui/edit/edit_vmess.ui
        ui/edit/edit_trojan_vless.h
        ui/edit/edit_trojan_vless.cpp
        ui/edit/edit_trojan_vless.ui

        ui/edit/edit_naive.h
        ui/edit/edit_naive.cpp
        ui/edit/edit_naive.ui

        ui/edit/edit_quic.h
        ui/edit/edit_quic.cpp
        ui/edit/edit_quic.ui

        ui/edit/edit_custom.h
        ui/edit/edit_custom.cpp
        ui/edit/edit_custom.ui

        ui/dialog_basic_settings.cpp
        ui/dialog_basic_settings.h
        ui/dialog_basic_settings.ui

        ui/dialog_manage_groups.cpp
        ui/dialog_manage_groups.h
        ui/dialog_manage_groups.ui

        ui/dialog_manage_routes.cpp
        ui/dialog_manage_routes.h
        ui/dialog_manage_routes.ui

        ui/dialog_vpn_settings.cpp
        ui/dialog_vpn_settings.h
        ui/dialog_vpn_settings.ui

        ui/dialog_hotkey.cpp
        ui/dialog_hotkey.h
        ui/dialog_hotkey.ui

        ui/widget/ProxyItem.cpp
        ui/widget/ProxyItem.h
        ui/widget/ProxyItem.ui
        ui/widget/GroupItem.cpp
        ui/widget/GroupItem.h
        ui/widget/GroupItem.ui

        res/proxor.qrc
        res/theme/feiyangqingyun/qss.qrc
        ${QV2RAY_RC}
)

# Qt exe
if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(proxor
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
    )
    # Define target properties for Android with Qt 6 as:
    #    set_property(TARGET proxor APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else ()
    if (ANDROID)
        add_library(proxor SHARED
                ${PROJECT_SOURCES}
        )
        # Define properties for Android with Qt 5 after find_package() calls as:
        #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else ()
        add_executable(proxor
                ${PROJECT_SOURCES}
        )
    endif ()
endif ()

# Target

set_property(TARGET proxor PROPERTY AUTOUIC ON)
set_property(TARGET proxor PROPERTY AUTOMOC ON)
set_property(TARGET proxor PROPERTY AUTORCC ON)

set_target_properties(proxor PROPERTIES
        WIN32_EXECUTABLE TRUE
)


# Target Link

target_link_libraries(proxor PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Svg
        Threads::Threads
        ${NKR_EXTERNAL_TARGETS}
        ${PLATFORM_LIBRARIES}
)

if (QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(proxor)
endif ()
