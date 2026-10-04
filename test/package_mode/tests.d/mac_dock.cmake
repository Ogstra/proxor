# macOS Dock reopen policy (Qt Core only, runs on every runner).
add_executable(mac_reopen_policy_test
    ${CMAKE_CURRENT_LIST_DIR}/../mac_reopen_policy_test.cpp
    ${PROXOR_SRC}/platform/MacReopenPolicy.cpp)
target_include_directories(mac_reopen_policy_test PRIVATE ${PROXOR_SRC})
target_link_libraries(mac_reopen_policy_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME mac_reopen_policy_test COMMAND mac_reopen_policy_test)
