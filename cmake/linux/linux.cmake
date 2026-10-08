find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS DBus)
set(PLATFORM_SOURCES src/sys/linux/LinuxCap.cpp src/sys/linux/LinuxSystemProxy.cpp
    src/sys/wifi/WifiBackendLinux.cpp src/sys/wifi/WifiBackendNetworkManager.cpp src/sys/wifi/WifiBackendIwd.cpp src/sys/wifi/WifiBackendChain.cpp
    src/sys/wifi/WifiPermissionNone.cpp
    src/sys/linux/XdgPortal.cpp src/sys/linux/XdgPortal.hpp src/sys/linux/DesktopPortalLinux.cpp
    src/sys/linux/PortalBackground.cpp src/sys/linux/PortalScreenshot.cpp src/sys/linux/PortalGlobalShortcuts.cpp
    src/sys/SleepWake.hpp src/sys/linux/LogindSleep.hpp src/sys/linux/LogindSleep.cpp
    src/ui/mainwindow_wake.cpp)
# Linux System Proxy is built from the shared desktop detection and planner instead of the 3rdparty Linux branch.
set(PLATFORM_REPLACED_SOURCES 3rdparty/qv2ray/v2/components/proxy/QvProxyConfigurator.cpp)
set(PLATFORM_LIBRARIES dl Qt${QT_VERSION_MAJOR}::DBus)
