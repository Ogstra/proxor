cmake_minimum_required(VERSION 3.10)
if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif ()
if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif ()

project(proxor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROXOR_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(PROXOR_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")

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
    set(NKR_NO_SQLITECPP 1)
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

# SQLiteCpp (static submodule)
if (NKR_NO_SQLITECPP)
    nkr_add_compile_definitions(NKR_NO_SQLITECPP)
else ()
    set(SQLITECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SQLITECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(3rdparty/SQLiteCpp)
    list(APPEND NKR_EXTERNAL_TARGETS SQLiteCpp)
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

        src/main/main.cpp
        src/main/ProxorGui.cpp
        src/main/ProxorGui_Utils.cpp
        src/main/HTTPRequestHelper.cpp

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

        src/rpc/gRPC.cpp

        src/db/Database.cpp
        src/db/traffic/TrafficLooper.cpp
        src/db/ProfileFilter.cpp
        src/db/ConfigBuilder.cpp

        src/fmt/AbstractBean.cpp
        src/fmt/Bean2CoreObj_box.cpp
        src/fmt/Bean2External.cpp
        src/fmt/Bean2Link.cpp
        src/fmt/Link2Bean.cpp
        src/fmt/ChainBean.hpp # translate

        src/sub/GroupUpdater.cpp

        src/sys/ExternalProcess.cpp
        src/sys/AutoRun.cpp

        src/ui/ThemeManager.cpp
        src/ui/Icon.cpp

        src/ui/mainwindow_grpc.cpp
        src/ui/mainwindow.cpp
        src/ui/mainwindow.h
        src/ui/mainwindow.ui

        src/ui/edit/dialog_edit_profile.h
        src/ui/edit/dialog_edit_profile.cpp
        src/ui/edit/dialog_edit_profile.ui
        src/ui/edit/dialog_edit_group.h
        src/ui/edit/dialog_edit_group.cpp
        src/ui/edit/dialog_edit_group.ui

        src/ui/edit/edit_chain.h
        src/ui/edit/edit_chain.cpp
        src/ui/edit/edit_chain.ui
        src/ui/edit/edit_socks_http.h
        src/ui/edit/edit_socks_http.cpp
        src/ui/edit/edit_socks_http.ui
        src/ui/edit/edit_shadowsocks.h
        src/ui/edit/edit_shadowsocks.cpp
        src/ui/edit/edit_shadowsocks.ui
        src/ui/edit/edit_vmess.h
        src/ui/edit/edit_vmess.cpp
        src/ui/edit/edit_vmess.ui
        src/ui/edit/edit_trojan_vless.h
        src/ui/edit/edit_trojan_vless.cpp
        src/ui/edit/edit_trojan_vless.ui

        src/ui/edit/edit_naive.h
        src/ui/edit/edit_naive.cpp
        src/ui/edit/edit_naive.ui

        src/ui/edit/edit_quic.h
        src/ui/edit/edit_quic.cpp
        src/ui/edit/edit_quic.ui

        src/ui/edit/edit_custom.h
        src/ui/edit/edit_custom.cpp
        src/ui/edit/edit_custom.ui

        src/ui/dialog_basic_settings.cpp
        src/ui/dialog_basic_settings.h
        src/ui/dialog_basic_settings.ui

        src/ui/dialog_manage_groups.cpp
        src/ui/dialog_manage_groups.h
        src/ui/dialog_manage_groups.ui

        src/ui/dialog_manage_routes.cpp
        src/ui/dialog_manage_routes.h
        src/ui/dialog_manage_routes.ui

        src/ui/dialog_vpn_settings.cpp
        src/ui/dialog_vpn_settings.h
        src/ui/dialog_vpn_settings.ui

        src/ui/dialog_ssid_settings.cpp
        src/ui/dialog_ssid_settings.h
        src/ui/dialog_ssid_settings.ui

        src/ui/dialog_hotkey.cpp
        src/ui/dialog_hotkey.h
        src/ui/dialog_hotkey.ui

        src/ui/widget/ProxyItem.cpp
        src/ui/widget/ProxyItem.h
        src/ui/widget/ProxyItem.ui
        src/ui/widget/GroupItem.cpp
        src/ui/widget/GroupItem.h
        src/ui/widget/GroupItem.ui

        assets/res/proxor.qrc
        assets/res/qdarkstyle/darkstyle.qrc
        ${QV2RAY_RC}
  )

set(PROXOR_GUI_TARGET proxor)

# Qt exe
if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    if (WIN32)
        set(PROXOR_GUI_TARGET proxor_gui)
    endif ()
    qt_add_executable(${PROXOR_GUI_TARGET}
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
    )
    # Define target properties for Android with Qt 6 as:
    #    set_property(TARGET proxor APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else ()
    if (ANDROID)
        add_library(${PROXOR_GUI_TARGET} SHARED
                ${PROJECT_SOURCES}
        )
        # Define properties for Android with Qt 5 after find_package() calls as:
        #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else ()
        add_executable(${PROXOR_GUI_TARGET}
                ${PROJECT_SOURCES}
        )
    endif ()
endif ()

# Target

set_property(TARGET ${PROXOR_GUI_TARGET} PROPERTY AUTOUIC ON)
set_property(TARGET ${PROXOR_GUI_TARGET} PROPERTY AUTOMOC ON)
set_property(TARGET ${PROXOR_GUI_TARGET} PROPERTY AUTORCC ON)

set_target_properties(${PROXOR_GUI_TARGET} PROPERTIES
        WIN32_EXECUTABLE TRUE
)

if (WIN32)
    set_target_properties(${PROXOR_GUI_TARGET} PROPERTIES OUTPUT_NAME "app")

    add_executable(proxor WIN32
            src/main/windows_runtime_launcher.cpp
            ${QV2RAY_RC}
    )
    set_target_properties(proxor PROPERTIES
            WIN32_EXECUTABLE TRUE
    )
    if (MSVC)
        set_property(TARGET proxor PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif ()
endif ()

# Target Link

target_link_libraries(${PROXOR_GUI_TARGET} PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Svg
        Threads::Threads
        ${NKR_EXTERNAL_TARGETS}
        ${PLATFORM_LIBRARIES}
)

target_include_directories(${PROXOR_GUI_TARGET} PRIVATE
        ${PROXOR_SRC_DIR}
        ${CMAKE_SOURCE_DIR}
)

if (WIN32)
    target_link_libraries(proxor PRIVATE
            ${PLATFORM_LIBRARIES}
            shell32
    )
endif ()

if (QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROXOR_GUI_TARGET})
endif ()
