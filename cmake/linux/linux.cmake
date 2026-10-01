find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS DBus)
set(PLATFORM_SOURCES src/sys/linux/LinuxCap.cpp
    src/sys/wifi/WifiBackendLinux.cpp src/sys/wifi/WifiBackendNetworkManager.cpp src/sys/wifi/WifiPermissionNone.cpp)
set(PLATFORM_LIBRARIES dl Qt${QT_VERSION_MAJOR}::DBus)
