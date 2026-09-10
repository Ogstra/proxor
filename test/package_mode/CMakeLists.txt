cmake_minimum_required(VERSION 3.21)
project(proxor_package_mode_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Core Test)

set(PACKAGE_MODE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../../src/main/PackageMode.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../src/main/PackagePolicy.cpp)

foreach(test_name IN ITEMS package_mode_test package_policy_test)
    add_executable(${test_name}
        ${test_name}.cpp
        ${PACKAGE_MODE_SOURCES})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../src/main)
    target_link_libraries(${test_name} PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
