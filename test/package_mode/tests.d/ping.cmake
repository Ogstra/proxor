# ICMP-to-TCP ping fallback policy (Qt Core only).
add_executable(ping_policy_test
    ${CMAKE_CURRENT_LIST_DIR}/../ping_policy_test.cpp
    ${PROXOR_SRC}/platform/PingPolicy.cpp)
target_include_directories(ping_policy_test PRIVATE ${PROXOR_SRC})
target_link_libraries(ping_policy_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ping_policy_test COMMAND ping_policy_test)
