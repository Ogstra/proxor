# macOS platform settings. Included from CMakeLists.txt before cmake/nkr.cmake,
# so this uses add_compile_definitions rather than nkr_add_compile_definitions.
enable_language(OBJCXX)
set(PLATFORM_SOURCES
    src/sys/DesktopPortalNone.cpp
    src/ui/mac/MacPlatform.h
    src/ui/mac/MacPlatform.mm
    src/ui/mac/MacLook.h
    src/ui/mac/MacLook.cpp
    src/ui/mac/MacLookCommon.h
    src/ui/mac/MacDialogs.h
    src/ui/mac/MacDialogs.cpp
    src/sys/macos/MacHelperPolicy.h
    src/sys/macos/MacHelperPolicy.cpp
    src/sys/macos/MacHelperClient.h
    src/sys/macos/MacHelperClient.cpp
    src/sys/macos/MacHelperService.h
    src/sys/macos/MacHelperService.cpp
    src/sys/macos/MacModeCoordinator.h
    src/sys/macos/MacModeCoordinator.cpp
    src/sys/macos/MacHelperInstaller.h
    src/sys/macos/MacHelperInstaller.cpp
    src/sys/macos/MacLoginItem.h
    src/sys/macos/MacLoginItem.mm
    src/sys/macos/MacScreenCapture.h
    src/sys/macos/MacScreenCapture.mm
    src/sys/wifi/WifiBackendMac.mm
    src/sys/wifi/WifiPermissionMac.mm
    assets/macos/proxor.icns
)
# App icon (Dock, Finder, Launchpad): copied into Contents/Resources and named in Info.plist.
set_source_files_properties(assets/macos/proxor.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
find_library(CORE_SERVICES_FRAMEWORK CoreServices REQUIRED)
find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
find_library(COREWLAN_FRAMEWORK CoreWLAN REQUIRED)
find_library(CORELOCATION_FRAMEWORK CoreLocation REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(SERVICE_MANAGEMENT_FRAMEWORK ServiceManagement REQUIRED)
find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
set_source_files_properties(src/sys/wifi/WifiBackendMac.mm src/sys/wifi/WifiPermissionMac.mm src/sys/macos/MacLoginItem.mm src/sys/macos/MacScreenCapture.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
set(PLATFORM_LIBRARIES ${CORE_FOUNDATION_FRAMEWORK} ${CORE_SERVICES_FRAMEWORK} ${APPKIT_FRAMEWORK} ${COREWLAN_FRAMEWORK} ${CORELOCATION_FRAMEWORK} ${FOUNDATION_FRAMEWORK} ${SERVICE_MANAGEMENT_FRAMEWORK} ${CORE_GRAPHICS_FRAMEWORK})
# Keep user config out of the signed bundle (Throne: NKR_PACKAGE_MACOS -> NKR_CPP_USE_APPDATA).
add_compile_definitions(NKR_CPP_USE_APPDATA)
add_link_options(-Wl,-dead_strip)
