#pragma once
#include <string>

// Send a Wake-on-LAN magic packet to the given MAC address.
// mac: "AA:BB:CC:DD:EE:FF"
// broadcast: subnet broadcast, default 255.255.255.255
bool send_wol(const std::string& mac, const std::string& broadcast = "255.255.255.255");
