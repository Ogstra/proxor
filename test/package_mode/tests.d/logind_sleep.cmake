# systemd-logind sleep/wake listener (QtCore + QtDBus) against a fake org.freedesktop.login1 on a private session bus.
find_package(Qt6 QUIET COMPONENTS DBus)
find_program(PROXOR_DBUS_RUN_SESSION dbus-run-session)
if (TARGET Qt6::DBus)
    add_executable(logind_sleep_test
        ${CMAKE_CURRENT_LIST_DIR}/../logind_sleep_test.cpp
        ${PROXOR_SRC}/sys/linux/LogindSleep.cpp
        ${PROXOR_SRC}/sys/linux/LogindSleep.hpp)
    target_include_directories(logind_sleep_test PRIVATE ${PROXOR_SRC})
    target_link_libraries(logind_sleep_test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    if (PROXOR_DBUS_RUN_SESSION AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME logind_sleep_test COMMAND ${PROXOR_DBUS_RUN_SESSION} -- $<TARGET_FILE:logind_sleep_test>)
    else ()
        message(STATUS "logind_sleep_test compiled, not run here (use test/package_mode/run-on-private-bus.sh)")
    endif ()
else ()
    message(STATUS "Qt6 DBus not found: logind_sleep_test skipped on this runner")
endif ()
