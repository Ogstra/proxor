# macOS Screen Recording gate for QR scans (phase 53). The wiring check is wiring.d/mac_qr.sh; the
# smoke test below runs on macOS only and calls the non-prompting preflight only.
if (APPLE)
    enable_language(OBJCXX)
    find_library(PROXOR_COREGRAPHICS CoreGraphics REQUIRED)
    find_library(PROXOR_APPKIT AppKit REQUIRED)
    find_library(PROXOR_FOUNDATION Foundation REQUIRED)
    add_executable(mac_screen_capture_smoke_test
        ${CMAKE_CURRENT_LIST_DIR}/../mac_screen_capture_smoke_test.cpp
        ${PROXOR_SRC}/sys/macos/MacScreenCapture.mm)
    set_source_files_properties(${PROXOR_SRC}/sys/macos/MacScreenCapture.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_include_directories(mac_screen_capture_smoke_test PRIVATE ${PROXOR_SRC})
    target_link_libraries(mac_screen_capture_smoke_test PRIVATE Qt6::Core Qt6::Gui Qt6::Test
        ${PROXOR_COREGRAPHICS} ${PROXOR_APPKIT} ${PROXOR_FOUNDATION})
    add_test(NAME mac_screen_capture_smoke_test COMMAND mac_screen_capture_smoke_test)
endif ()
