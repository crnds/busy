#include "Net.h"
#include "../settings/Settings.h"
#include "../../include/config.h"
#include <ESPmDNS.h>

void mdnsBegin() {
    if (!MDNS.begin(CFG.host)) {
        Serial.println("[mdns] begin failed");
        return;
    }
    MDNS.addService("http", "tcp", 80);
    // Advertised as the original does, so BUSY Bar discovery tooling finds us.
    MDNS.addService("busybar", "tcp", 80);
    MDNS.addServiceTxt("busybar", "tcp", "fw", FW_NAME);
    MDNS.addServiceTxt("busybar", "tcp", "ver", FW_VERSION);
    MDNS.addServiceTxt("busybar", "tcp", "api", FW_API_COMPAT);
    Serial.printf("[mdns] http://%s.local\n", CFG.host);
}
