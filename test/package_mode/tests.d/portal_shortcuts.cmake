# Key sequence -> desktop shortcut trigger (Qt Core only, every runner).
add_executable(portal_trigger_test
    ${CMAKE_CURRENT_LIST_DIR}/../portal_trigger_test.cpp
    ${PROXOR_SRC}/platform/PortalShortcutTrigger.cpp)
target_include_directories(portal_trigger_test PRIVATE ${PROXOR_SRC})
target_link_libraries(portal_trigger_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME portal_trigger_test COMMAND portal_trigger_test)

# GlobalShortcuts session (QtCore + QtDBus) against the shared fake portal on a private session bus.
find_package(Qt6 QUIET COMPONENTS DBus)
find_program(PROXOR_DBUS_RUN_SESSION dbus-run-session)
if (TARGET Qt6::DBus)
    add_executable(portal_shortcuts_test
        ${CMAKE_CURRENT_LIST_DIR}/../portal_shortcuts_test.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../fake_portal.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../fake_portal.hpp
        ${PROXOR_SRC}/sys/linux/XdgPortal.cpp
        ${PROXOR_SRC}/sys/linux/PortalGlobalShortcuts.cpp
        ${PROXOR_SRC}/sys/linux/DesktopPortalLinux.cpp)
    target_include_directories(portal_shortcuts_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/sys/linux ${CMAKE_CURRENT_LIST_DIR}/..)
    target_link_libraries(portal_shortcuts_test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    if (PROXOR_DBUS_RUN_SESSION AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME portal_shortcuts_test COMMAND ${PROXOR_DBUS_RUN_SESSION} -- $<TARGET_FILE:portal_shortcuts_test>)
    else ()
        message(STATUS "portal_shortcuts_test compiled, not run here (use test/package_mode/run-on-private-bus.sh)")
    endif ()
else ()
    message(STATUS "Qt6 DBus not found: portal_shortcuts_test skipped on this runner")
endif ()
