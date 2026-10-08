# Linux autostart command and desktop entry (Qt Core only, pure).
add_executable(autostart_test
    ${CMAKE_CURRENT_LIST_DIR}/../autostart_test.cpp
    ${PROXOR_SRC}/platform/LinuxAutostart.cpp
    ${PROXOR_SRC}/main/PackageMode.cpp)
target_include_directories(autostart_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/main)
target_link_libraries(autostart_test PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(autostart_test PRIVATE
    PROXOR_AUTOSTART_FIXTURE="${CMAKE_CURRENT_LIST_DIR}/../fixtures/linux-autostart-native.desktop")
add_test(NAME autostart_test COMMAND autostart_test)
