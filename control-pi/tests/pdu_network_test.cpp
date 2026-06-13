#include "config.h"
#include "pdu/synlink_client.h"
#include <cstdio>

int main(int argc, char** argv) {
    const char* config_path = argc > 1 ? argv[1] : "config.toml";
    auto cfg = Config::load(config_path);
    if (!cfg.pdu.enabled) {
        std::fprintf(stderr, "PDU is disabled in %s\n", config_path);
        return 1;
    }

    SynlinkClient client(cfg.pdu);
    auto snapshot = client.get_snapshot();
    if (!snapshot || snapshot->outlets.empty()) {
        std::fprintf(stderr, "PDU query failed for %s:%u\n",
                     cfg.pdu.host.c_str(), cfg.pdu.port);
        return 1;
    }

    std::printf("Connected to %s:%u; received %zu outlets\n",
                cfg.pdu.host.c_str(), cfg.pdu.port, snapshot->outlets.size());
    if (snapshot->measurements_available)
        std::printf("Estimated rack load: %.1fW (%.3fA x %.1fV nominal)\n",
                    snapshot->estimated_watts, snapshot->inlet_amps, snapshot->nominal_volts);
    for (const auto& outlet : snapshot->outlets) {
        std::printf("  %d: %s [%s]\n",
                    outlet.outlet, outlet.name.c_str(), outlet.on ? "ON" : "OFF");
    }
    return 0;
}
