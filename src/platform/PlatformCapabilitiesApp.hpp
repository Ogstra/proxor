#pragma once

// App-only glue (reads QGuiApplication and ProxorGui globals); never compiled into unit tests.

#include "platform/PlatformCapabilities.hpp"

namespace ProxorPlatform {

PlatformEnvironment CurrentPlatformEnvironment();
CapabilityStatus CurrentCapability(Capability capability);

} // namespace ProxorPlatform
