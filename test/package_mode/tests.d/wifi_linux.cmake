# NetworkManager Wi-Fi reader (QtCore + QtDBus): fake NetworkManager on a private session bus + fake nmcli.
find_package(Qt6 QUIET COMPONENTS DBus)
find_program(PROXOR_DBUS_RUN_SESSION dbus-run-session)
if (TARGET Qt6::DBus)
    add_executable(wifi_nm_test
        ${CMAKE_CURRENT_LIST_DIR}/../wifi_nm_test.cpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendNetworkManager.cpp
        ${PROXOR_SRC}/platform/WifiSsid.cpp)
    target_include_directories(wifi_nm_test PRIVATE ${PROXOR_SRC})
    target_link_libraries(wifi_nm_test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    # Homebrew dbus-run-session needs launchd and cannot start a private bus on macOS; run it on Linux only.
    if (PROXOR_DBUS_RUN_SESSION AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME wifi_nm_test COMMAND ${PROXOR_DBUS_RUN_SESSION} -- $<TARGET_FILE:wifi_nm_test>)
    else ()
        message(STATUS "dbus-run-session unusable here: wifi_nm_test is compiled but not run on this runner")
    endif ()
else ()
    message(STATUS "Qt6 DBus not found: wifi_nm_test skipped on this runner")
endif ()
