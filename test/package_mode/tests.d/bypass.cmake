# Auto-bypass of external cores and VPN clients per OS (Qt Core only).
add_executable(auto_bypass_test
    ${CMAKE_CURRENT_LIST_DIR}/../auto_bypass_test.cpp
    ${PROXOR_SRC}/platform/AutoBypass.cpp
    ${PROXOR_SRC}/platform/PlatformCapabilities.cpp
    ${PROXOR_SRC}/main/PackageMode.cpp)
target_include_directories(auto_bypass_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/main)
target_link_libraries(auto_bypass_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME auto_bypass_test COMMAND auto_bypass_test)
