# macOS Wi-Fi: pure classification on every runner; real CoreWLAN smoke test on macOS only (phase 51).
add_executable(wifi_mac_classify_test
    ${CMAKE_CURRENT_LIST_DIR}/../wifi_mac_classify_test.cpp
    ${PROXOR_SRC}/platform/WifiMacClassify.cpp
    ${PROXOR_SRC}/platform/WifiSsid.cpp)
target_include_directories(wifi_mac_classify_test PRIVATE ${PROXOR_SRC})
target_link_libraries(wifi_mac_classify_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME wifi_mac_classify_test COMMAND wifi_mac_classify_test)
