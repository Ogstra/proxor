# NetworkManager-then-iwd Wi-Fi chain (QtCore + QtDBus): pure choice table + fake iwd on a private session bus.
find_package(Qt6 QUIET COMPONENTS DBus)
find_program(PROXOR_DBUS_RUN_SESSION dbus-run-session)
if (TARGET Qt6::DBus)
    add_executable(wifi_chain_test
        ${CMAKE_CURRENT_LIST_DIR}/../wifi_chain_test.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../fake_iwd.hpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendChain.cpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendChain.hpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendIwd.cpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendIwd.hpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendNetworkManager.cpp
        ${PROXOR_SRC}/platform/WifiSsid.cpp)
    target_include_directories(wifi_chain_test PRIVATE ${PROXOR_SRC} ${CMAKE_CURRENT_LIST_DIR}/..)
    target_link_libraries(wifi_chain_test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    if (PROXOR_DBUS_RUN_SESSION AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME wifi_chain_test COMMAND ${PROXOR_DBUS_RUN_SESSION} -- $<TARGET_FILE:wifi_chain_test>)
    else ()
        message(STATUS "wifi_chain_test compiled, not run here (use test/package_mode/run-on-private-bus.sh)")
    endif ()
else ()
    message(STATUS "Qt6 DBus not found: wifi_chain_test skipped on this runner")
endif ()
