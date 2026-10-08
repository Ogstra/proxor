# systemd-resolved /etc/resolv.conf classification (Qt Core only).
add_executable(resolv_conf_test
    ${CMAKE_CURRENT_LIST_DIR}/../resolv_conf_test.cpp
    ${PROXOR_SRC}/platform/ResolvConf.cpp)
target_include_directories(resolv_conf_test PRIVATE ${PROXOR_SRC})
target_link_libraries(resolv_conf_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME resolv_conf_test COMMAND resolv_conf_test)
