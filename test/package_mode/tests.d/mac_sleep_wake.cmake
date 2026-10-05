# macOS native sleep/wake observer: in-process NSWorkspace notifications (never sleeps the Mac). macOS only.
if (APPLE)
    enable_language(OBJCXX)
    find_library(PROXOR_APPKIT AppKit REQUIRED)
    find_library(PROXOR_FOUNDATION Foundation REQUIRED)
    add_executable(mac_sleep_wake_smoke_test
        ${CMAKE_CURRENT_LIST_DIR}/../mac_sleep_wake_smoke_test.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../mac_sleep_wake_post.mm
        ${PROXOR_SRC}/sys/macos/MacSleepWake.mm)
    set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/../mac_sleep_wake_post.mm ${PROXOR_SRC}/sys/macos/MacSleepWake.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_include_directories(mac_sleep_wake_smoke_test PRIVATE ${PROXOR_SRC})
    target_link_libraries(mac_sleep_wake_smoke_test PRIVATE Qt6::Core Qt6::Test ${PROXOR_APPKIT} ${PROXOR_FOUNDATION})
    add_test(NAME mac_sleep_wake_smoke_test COMMAND mac_sleep_wake_smoke_test)
endif ()
