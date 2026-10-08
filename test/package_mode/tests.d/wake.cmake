# Wake coordinator policy (Qt Core only).
add_executable(wake_coordinator_test
    ${CMAKE_CURRENT_LIST_DIR}/../wake_coordinator_test.cpp
    ${PROXOR_SRC}/platform/WakeCoordinator.cpp)
target_include_directories(wake_coordinator_test PRIVATE ${PROXOR_SRC})
target_link_libraries(wake_coordinator_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME wake_coordinator_test COMMAND wake_coordinator_test)
