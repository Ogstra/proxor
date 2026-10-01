# Linux desktop detection and system proxy planning (Qt Core only; runs on every runner).
add_executable(linux_desktop_test
    ${CMAKE_CURRENT_LIST_DIR}/../linux_desktop_test.cpp
    ${PROXOR_SRC}/platform/LinuxDesktop.cpp)
target_include_directories(linux_desktop_test PRIVATE ${PROXOR_SRC})
target_link_libraries(linux_desktop_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME linux_desktop_test COMMAND linux_desktop_test)

add_executable(linux_system_proxy_test
    ${CMAKE_CURRENT_LIST_DIR}/../linux_system_proxy_test.cpp
    ${PROXOR_SRC}/platform/LinuxDesktop.cpp
    ${PROXOR_SRC}/platform/LinuxSystemProxyPlan.cpp)
target_include_directories(linux_system_proxy_test PRIVATE ${PROXOR_SRC})
target_link_libraries(linux_system_proxy_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME linux_system_proxy_test COMMAND linux_system_proxy_test)
