# Platform capability truth table (Qt Core only) and its disable-and-explain widget helper.
add_executable(platform_capabilities_test
    ${CMAKE_CURRENT_LIST_DIR}/../platform_capabilities_test.cpp
    ${PROXOR_SRC}/platform/PlatformCapabilities.cpp
    ${PROXOR_SRC}/main/PackageMode.cpp)
target_include_directories(platform_capabilities_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/main)
target_link_libraries(platform_capabilities_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME platform_capabilities_test COMMAND platform_capabilities_test)

find_package(Qt6 REQUIRED COMPONENTS Gui Widgets)
add_executable(capability_ui_test
    ${CMAKE_CURRENT_LIST_DIR}/../capability_ui_test.cpp
    ${PROXOR_SRC}/platform/CapabilityUi.cpp
    ${PROXOR_SRC}/platform/PlatformCapabilities.cpp
    ${PROXOR_SRC}/main/PackageMode.cpp)
target_include_directories(capability_ui_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/main)
target_link_libraries(capability_ui_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME capability_ui_test COMMAND capability_ui_test)
set_tests_properties(capability_ui_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
