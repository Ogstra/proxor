# Compact menu-bar traffic rate text (Qt Core only).
add_executable(tray_speed_test
    ${CMAKE_CURRENT_LIST_DIR}/../tray_speed_test.cpp
    ${PROXOR_SRC}/platform/TraySpeed.cpp)
target_include_directories(tray_speed_test PRIVATE ${PROXOR_SRC})
target_link_libraries(tray_speed_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tray_speed_test COMMAND tray_speed_test)
