# Key sequence -> desktop shortcut trigger (Qt Core only, every runner).
add_executable(portal_trigger_test
    ${CMAKE_CURRENT_LIST_DIR}/../portal_trigger_test.cpp
    ${PROXOR_SRC}/platform/PortalShortcutTrigger.cpp)
target_include_directories(portal_trigger_test PRIVATE ${PROXOR_SRC})
target_link_libraries(portal_trigger_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME portal_trigger_test COMMAND portal_trigger_test)
