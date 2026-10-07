# Linux Wi-Fi factory (QtCore + QtDBus): builds the NetworkManager-then-iwd chain and answers one read in time.
# Needs no bus of its own (never registers a service); without a system bus both readers take their fast path.
find_package(Qt6 QUIET COMPONENTS DBus)
if (TARGET Qt6::DBus)
    add_executable(wifi_factory_test
        ${CMAKE_CURRENT_LIST_DIR}/../wifi_factory_test.cpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendLinux.cpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendChain.cpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendIwd.cpp
        ${PROXOR_SRC}/sys/wifi/WifiBackendNetworkManager.cpp
        ${PROXOR_SRC}/platform/WifiSsid.cpp)
    target_include_directories(wifi_factory_test PRIVATE ${PROXOR_SRC})
    target_link_libraries(wifi_factory_test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    add_test(NAME wifi_factory_test COMMAND wifi_factory_test)
else ()
    message(STATUS "Qt6 DBus not found: wifi_factory_test skipped on this runner")
endif ()
