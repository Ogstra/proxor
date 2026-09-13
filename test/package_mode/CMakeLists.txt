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

# Kept separate from the foreach above: this target needs Widgets/Gui and the dialog's
# own sources rather than the PackageMode/PackagePolicy sources. dialog_update_available.cpp
# has no project dependencies of its own -- only QPushButton, QApplication, QPalette and
# QRegularExpression plus its generated ui_ header -- which is what makes it linkable here
# standalone.
find_package(Qt6 REQUIRED COMPONENTS Gui Widgets)
set(CMAKE_AUTOUIC ON)

add_executable(dialog_update_available_test
    dialog_update_available_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../src/ui/dialog_update_available.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../src/ui/dialog_update_available.ui)
target_include_directories(dialog_update_available_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../src/ui)
target_link_libraries(dialog_update_available_test PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME dialog_update_available_test COMMAND dialog_update_available_test)
set_tests_properties(dialog_update_available_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
