# Hotkey registration planning (Qt Core only).
add_executable(hotkey_report_test
    ${CMAKE_CURRENT_LIST_DIR}/../hotkey_report_test.cpp
    ${PROXOR_SRC}/platform/HotkeyReport.cpp
    ${PROXOR_SRC}/platform/PlatformCapabilities.cpp
    ${PROXOR_SRC}/main/PackageMode.cpp)
target_include_directories(hotkey_report_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/main)
target_link_libraries(hotkey_report_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME hotkey_report_test COMMAND hotkey_report_test)
