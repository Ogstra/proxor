#include "sys/macos/MacLocalNetwork.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

void MacLocalNetwork::TriggerPrompt() {
    // The service type must also be listed under NSBonjourServices in Info.plist (CMakeLists.txt),
    // otherwise the browse fails without showing the prompt.
    nw_browse_descriptor_t descriptor = nw_browse_descriptor_create_bonjour_service("_proxor-probe._tcp", "local");
    nw_parameters_t parameters = nw_parameters_create();
    nw_browser_t browser = nw_browser_create(descriptor, parameters);
    dispatch_queue_t queue = dispatch_queue_create("io.github.Ogstra.Proxor.localnetwork", DISPATCH_QUEUE_SERIAL);
    nw_browser_set_queue(browser, queue);
    nw_browser_set_browse_results_changed_handler(browser, ^(nw_browse_result_t, nw_browse_result_t, bool) {});
    nw_browser_start(browser);
    // Keep the browse alive while the prompt is on screen, then stop it.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 60 * NSEC_PER_SEC), queue, ^{
        nw_browser_cancel(browser);
    });
}
