# Tray close/startup policy (Qt Core only) over the SystemTray capability row.
add_executable(tray_policy_test
    ${CMAKE_CURRENT_LIST_DIR}/../tray_policy_test.cpp
    ${PROXOR_SRC}/platform/TrayPolicy.cpp
    ${PROXOR_SRC}/platform/PlatformCapabilities.cpp
    ${PROXOR_SRC}/main/PackageMode.cpp)
target_include_directories(tray_policy_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/main)
target_link_libraries(tray_policy_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tray_policy_test COMMAND tray_policy_test)
