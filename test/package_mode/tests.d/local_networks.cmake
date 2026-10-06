# Attached private networks instead of blanket RFC 1918 exclusions (Qt Core only).
add_executable(local_networks_test
    ${CMAKE_CURRENT_LIST_DIR}/../local_networks_test.cpp
    ${PROXOR_SRC}/platform/LocalNetworks.cpp)
target_include_directories(local_networks_test PRIVATE ${PROXOR_SRC})
target_link_libraries(local_networks_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME local_networks_test COMMAND local_networks_test)
