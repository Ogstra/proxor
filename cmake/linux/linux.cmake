find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS DBus)
set(PLATFORM_SOURCES src/sys/linux/LinuxCap.cpp src/sys/linux/LinuxSystemProxy.cpp
    src/sys/wifi/WifiBackendLinux.cpp src/sys/wifi/WifiBackendNetworkManager.cpp src/sys/wifi/WifiPermissionNone.cpp)
# Linux System Proxy is built from the shared desktop detection and planner instead of the 3rdparty Linux branch.
set(PLATFORM_REPLACED_SOURCES 3rdparty/qv2ray/v2/components/proxy/QvProxyConfigurator.cpp)
set(PLATFORM_LIBRARIES dl Qt${QT_VERSION_MAJOR}::DBus)
