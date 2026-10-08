# macOS in-app update (phase 60): the route, staging dir, relauncher arguments and result parsing are pure Qt Core and run on every runner.
add_executable(mac_app_update_policy_test
    ${CMAKE_CURRENT_LIST_DIR}/../mac_app_update_policy_test.cpp
    ${PROXOR_SRC}/platform/MacAppUpdatePolicy.cpp)
target_include_directories(mac_app_update_policy_test PRIVATE ${PROXOR_SRC})
target_link_libraries(mac_app_update_policy_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME mac_app_update_policy_test COMMAND mac_app_update_policy_test)
