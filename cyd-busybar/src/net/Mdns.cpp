#include "net/Mdns.h"
#include "net/WifiSetup.h"
#include <ESPmDNS.h>

void Mdns::begin() {
  if (!WifiSetup::connected()) return;
  if (!MDNS.begin("cyd-busybar")) return;
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("busybar", "tcp", 80);
  MDNS.addServiceTxt("busybar", "tcp", "path", "/");
  MDNS.addServiceTxt("busybar", "tcp", "fw", "cyd-busybar");
}

void Mdns::end() { MDNS.end(); }
