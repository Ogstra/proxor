# Tun address list and Linux IPv6 policy (Qt Core only).
add_executable(tun_address_test
    ${CMAKE_CURRENT_LIST_DIR}/../tun_address_test.cpp
    ${PROXOR_SRC}/platform/TunAddress.cpp)
target_include_directories(tun_address_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/main)
target_link_libraries(tun_address_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tun_address_test COMMAND tun_address_test)
