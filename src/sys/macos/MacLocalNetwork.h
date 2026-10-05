#pragma once

// macOS asks "Allow Proxor to find devices on local networks?" the first time an app touches the
// local network. Triggering a short Bonjour browse at the first start makes that native prompt
// appear then, instead of in the middle of a connection attempt. Safe to call more than once.
namespace MacLocalNetwork {
void TriggerPrompt();
}
