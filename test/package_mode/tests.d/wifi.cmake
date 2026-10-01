# Wi-Fi SSID module (Qt Core only) and the asynchronous WifiMonitor with fake backends (phase 51).
add_executable(wifi_ssid_test
    ${CMAKE_CURRENT_LIST_DIR}/../wifi_ssid_test.cpp
    ${PROXOR_SRC}/platform/WifiSsid.cpp)
target_include_directories(wifi_ssid_test PRIVATE ${PROXOR_SRC})
target_link_libraries(wifi_ssid_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME wifi_ssid_test COMMAND wifi_ssid_test)
