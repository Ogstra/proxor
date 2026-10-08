# QR scan message policy (Qt Core only) and ZXing image decoding.
add_executable(qr_scan_policy_test ${CMAKE_CURRENT_LIST_DIR}/../qr_scan_policy_test.cpp
    ${PROXOR_SRC}/platform/QrScanPolicy.cpp ${PROXOR_SRC}/platform/PlatformCapabilities.cpp ${PROXOR_SRC}/main/PackageMode.cpp)
target_include_directories(qr_scan_policy_test PRIVATE ${PROXOR_SRC} ${PROXOR_SRC}/main)
target_link_libraries(qr_scan_policy_test PRIVATE Qt6::Core Qt6::Test)
target_sources(qr_scan_policy_test PRIVATE ${PROXOR_SRC}/platform/LinuxDesktop.cpp)
add_test(NAME qr_scan_policy_test COMMAND qr_scan_policy_test)
# The decoder needs zxing-cpp; runners without it (the Windows/Linux policy runners) skip this target, the
# macOS runner (brew zxing-cpp) and this Mac build it.
find_package(ZXing CONFIG QUIET)
if (ZXing_FOUND)
    find_package(Qt6 REQUIRED COMPONENTS Gui)
    add_executable(qr_image_decode_test ${CMAKE_CURRENT_LIST_DIR}/../qr_image_decode_test.cpp
        ${PROXOR_SRC}/platform/QrImageDecode.cpp ${CMAKE_CURRENT_LIST_DIR}/../../../3rdparty/qrcodegen.cpp)
    target_include_directories(qr_image_decode_test PRIVATE ${PROXOR_SRC} ${CMAKE_CURRENT_LIST_DIR}/../../..)
    target_link_libraries(qr_image_decode_test PRIVATE Qt6::Core Qt6::Gui Qt6::Test ZXing::ZXing)
    add_test(NAME qr_image_decode_test COMMAND qr_image_decode_test)
    set_tests_properties(qr_image_decode_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
else ()
    message(STATUS "zxing-cpp not found: qr_image_decode_test skipped on this runner")
endif ()
