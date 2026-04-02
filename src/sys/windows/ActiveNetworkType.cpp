#include "ActiveNetworkType.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

namespace ProxorGui_sys {
    ActiveNetworkType DetectActiveNetworkType() {
        IN_ADDR destinationAddress{};
        if (InetPtonA(AF_INET, "1.1.1.1", &destinationAddress) != 1) return ActiveNetworkType::Unknown;

        DWORD interfaceIndex = 0;
        if (GetBestInterface(destinationAddress.S_un.S_addr, &interfaceIndex) != NO_ERROR) {
            return ActiveNetworkType::Unknown;
        }

        MIB_IFROW row{};
        row.dwIndex = interfaceIndex;
        if (GetIfEntry(&row) != NO_ERROR) {
            return ActiveNetworkType::Unknown;
        }

        if (row.dwType == IF_TYPE_IEEE80211) return ActiveNetworkType::Wifi;
        if (row.dwType == IF_TYPE_ETHERNET_CSMACD) return ActiveNetworkType::Ethernet;
        return ActiveNetworkType::Unknown;
    }
}
