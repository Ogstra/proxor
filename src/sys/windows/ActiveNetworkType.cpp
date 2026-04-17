#include "ActiveNetworkType.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <vector>

#pragma comment(lib, "iphlpapi.lib")

namespace ProxorGui_sys {
    ActiveNetworkType DetectActiveNetworkType() {
        ULONG bufSize = 15000;
        std::vector<BYTE> buf(bufSize);
        PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());

        ULONG ret = GetAdaptersAddresses(
            AF_UNSPEC,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr,
            adapters,
            &bufSize);

        if (ret == ERROR_BUFFER_OVERFLOW) {
            buf.resize(bufSize);
            adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
            ret = GetAdaptersAddresses(
                AF_UNSPEC,
                GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                nullptr,
                adapters,
                &bufSize);
        }

        if (ret != NO_ERROR) return ActiveNetworkType::Unknown;

        bool hasWifi = false;
        bool hasEthernet = false;

        for (auto *a = adapters; a != nullptr; a = a->Next) {
            if (a->OperStatus != IfOperStatusUp) continue;
            if (a->FirstUnicastAddress == nullptr) continue;

            if (a->IfType == IF_TYPE_IEEE80211) {
                hasWifi = true;
            } else if (a->IfType == IF_TYPE_ETHERNET_CSMACD) {
                hasEthernet = true;
            }
        }

        if (hasWifi) return ActiveNetworkType::Wifi;
        if (hasEthernet) return ActiveNetworkType::Ethernet;
        return ActiveNetworkType::Unknown;
    }
}
