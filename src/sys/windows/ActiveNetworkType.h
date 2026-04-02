#pragma once

namespace ProxorGui_sys {
    enum class ActiveNetworkType {
        Unknown,
        Wifi,
        Ethernet,
    };

    ActiveNetworkType DetectActiveNetworkType();
}
