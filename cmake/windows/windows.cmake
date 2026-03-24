set(PLATFORM_SOURCES 3rdparty/WinCommander.cpp src/sys/windows/guihelper.cpp src/sys/windows/MiniDump.cpp)
set(PLATFORM_LIBRARIES wininet wsock32 ws2_32 user32 rasapi32 iphlpapi)

include(cmake/windows/generate_product_version.cmake)
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" NKR_WINDOWS_VERSION LIMIT_COUNT 1)
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" NKR_WINDOWS_VERSION_MATCH "${NKR_WINDOWS_VERSION}")
set(NKR_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(NKR_VERSION_MINOR "${CMAKE_MATCH_2}")
set(NKR_VERSION_PATCH "${CMAKE_MATCH_3}")
if (NOT NKR_VERSION_MAJOR)
    set(NKR_VERSION_MAJOR 1)
endif ()
if (NOT NKR_VERSION_MINOR)
    set(NKR_VERSION_MINOR 0)
endif ()
if (NOT NKR_VERSION_PATCH)
    set(NKR_VERSION_PATCH 0)
endif ()
generate_product_version(
        QV2RAY_RC
        ICON "${CMAKE_SOURCE_DIR}/assets/res/proxor.ico"
        NAME "Proxor"
        BUNDLE "proxor"
        VERSION_MAJOR "${NKR_VERSION_MAJOR}"
        VERSION_MINOR "${NKR_VERSION_MINOR}"
        VERSION_PATCH "${NKR_VERSION_PATCH}"
        VERSION_REVISION 0
        COMPANY_NAME "Proxor"
        COMPANY_COPYRIGHT "Proxor"
        FILE_DESCRIPTION "Proxor"
)
add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
set(GUI_TYPE WIN32)
if (MINGW)
    if (NOT DEFINED MinGW_ROOT)
        set(MinGW_ROOT "C:/msys64/mingw64")
    endif ()
else ()
    add_compile_options("/utf-8")
    add_compile_options("/std:c++17")
    add_definitions(-D_WIN32_WINNT=0x600 -D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
endif ()
