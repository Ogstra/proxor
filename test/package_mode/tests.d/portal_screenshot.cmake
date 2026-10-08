# Screenshot portal against the shared fake org.freedesktop.portal.Desktop on a private session bus.
find_package(Qt6 QUIET COMPONENTS DBus)
find_program(PROXOR_DBUS_RUN_SESSION dbus-run-session)
if (TARGET Qt6::DBus)
    add_executable(portal_screenshot_test
        ${CMAKE_CURRENT_LIST_DIR}/../portal_screenshot_test.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../fake_portal.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../fake_portal.hpp
        ${PROXOR_SRC}/sys/linux/XdgPortal.cpp
        ${PROXOR_SRC}/sys/linux/DesktopPortalLinux.cpp
        ${PROXOR_SRC}/sys/linux/PortalScreenshot.cpp)
    target_include_directories(portal_screenshot_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/sys/linux ${CMAKE_CURRENT_LIST_DIR}/..)
    target_link_libraries(portal_screenshot_test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    if (PROXOR_DBUS_RUN_SESSION AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME portal_screenshot_test COMMAND ${PROXOR_DBUS_RUN_SESSION} -- $<TARGET_FILE:portal_screenshot_test>)
    else ()
        message(STATUS "portal_screenshot tests compiled, not run here (use test/package_mode/run-on-private-bus.sh)")
    endif ()
else ()
    message(STATUS "Qt6 DBus not found: portal_screenshot_test skipped on this runner")
endif ()
