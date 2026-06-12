#include "host_ctrl.h"
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

protocol::CommandResult host_ctrl::reboot(const std::string& cmd_id) {
    // Schedule reboot in 1 second so we can send the ACK first
    int rc = system("systemctl reboot &");
    return { "cmd_result", cmd_id, rc == 0, rc == 0 ? "rebooting" : "systemctl reboot failed" };
}

protocol::CommandResult host_ctrl::shutdown(const std::string& cmd_id) {
    int rc = system("systemctl poweroff &");
    return { "cmd_result", cmd_id, rc == 0, rc == 0 ? "shutting down" : "systemctl poweroff failed" };
}

bool host_ctrl::send_wol(const std::string& mac, const std::string& broadcast) {
    // Parse MAC "AA:BB:CC:DD:EE:FF"
    uint8_t mac_bytes[6];
    if (sscanf(mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &mac_bytes[0], &mac_bytes[1], &mac_bytes[2],
               &mac_bytes[3], &mac_bytes[4], &mac_bytes[5]) != 6)
        return false;

    // Magic packet: 6×0xFF then 16× MAC
    uint8_t pkt[102];
    memset(pkt, 0xFF, 6);
    for (int i = 0; i < 16; ++i)
        memcpy(pkt + 6 + i * 6, mac_bytes, 6);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return false;

    int bcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(9);
    addr.sin_addr.s_addr = inet_addr(broadcast.c_str());

    bool ok = sendto(sock, pkt, sizeof(pkt), 0,
                     (sockaddr*)&addr, sizeof(addr)) == sizeof(pkt);
    close(sock);
    return ok;
}
