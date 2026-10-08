# Linux process-name reader over a fake /proc tree (Qt Core only).
add_executable(process_names_test
    ${CMAKE_CURRENT_LIST_DIR}/../process_names_test.cpp
    ${PROXOR_SRC}/platform/ProcessNames.cpp)
target_include_directories(process_names_test PRIVATE ${PROXOR_SRC})
target_link_libraries(process_names_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME process_names_test COMMAND process_names_test)
