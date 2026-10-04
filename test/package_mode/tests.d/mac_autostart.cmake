# macOS Start with system (phase 53): the LaunchAgent text and the checkbox truth table are pure Qt Core
# and run on every runner; the real login item module is a macOS-only smoke test (temp dir only).
add_executable(mac_login_item_policy_test
    ${CMAKE_CURRENT_LIST_DIR}/../mac_login_item_policy_test.cpp
    ${PROXOR_SRC}/platform/MacLoginItemPolicy.cpp)
target_include_directories(mac_login_item_policy_test PRIVATE ${PROXOR_SRC})
target_link_libraries(mac_login_item_policy_test PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(mac_login_item_policy_test PRIVATE
    PROXOR_MAC_AGENT_FIXTURE="${CMAKE_CURRENT_LIST_DIR}/../fixtures/macos-launch-agent.plist")
add_test(NAME mac_login_item_policy_test COMMAND mac_login_item_policy_test)

if (APPLE)
    enable_language(OBJCXX)
    find_library(PROXOR_SERVICE_MANAGEMENT ServiceManagement REQUIRED)
    find_library(PROXOR_APPKIT AppKit REQUIRED)
    find_library(PROXOR_FOUNDATION Foundation REQUIRED)
    add_executable(mac_login_item_smoke_test
        ${CMAKE_CURRENT_LIST_DIR}/../mac_login_item_smoke_test.cpp
        ${PROXOR_SRC}/sys/macos/MacLoginItem.mm
        ${PROXOR_SRC}/platform/MacLoginItemPolicy.cpp)
    set_source_files_properties(${PROXOR_SRC}/sys/macos/MacLoginItem.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_include_directories(mac_login_item_smoke_test PRIVATE ${PROXOR_SRC})
    target_link_libraries(mac_login_item_smoke_test PRIVATE Qt6::Core Qt6::Test
        ${PROXOR_SERVICE_MANAGEMENT} ${PROXOR_APPKIT} ${PROXOR_FOUNDATION})
    add_test(NAME mac_login_item_smoke_test COMMAND mac_login_item_smoke_test)
endif ()
