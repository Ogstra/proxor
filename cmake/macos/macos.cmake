# macOS platform settings. Included from CMakeLists.txt before cmake/nkr.cmake,
# so this uses add_compile_definitions rather than nkr_add_compile_definitions.
set(PLATFORM_SOURCES "")
find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
find_library(CORE_SERVICES_FRAMEWORK CoreServices REQUIRED)
set(PLATFORM_LIBRARIES ${CORE_FOUNDATION_FRAMEWORK} ${CORE_SERVICES_FRAMEWORK})
# Keep user config out of the signed bundle (Throne: NKR_PACKAGE_MACOS -> NKR_CPP_USE_APPDATA).
add_compile_definitions(NKR_CPP_USE_APPDATA)
add_link_options(-Wl,-dead_strip)
