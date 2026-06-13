#include "activity_store.h"
#include "command_router.h"
#include "config.h"
#include "metrics_store.h"
#include "pdu/pdu_store.h"
#include "power/power_history_store.h"
#include "power/power_budget_controller.h"
#include "power/power_sequence_engine.h"
#include <cassert>
#include <ctime>
#include <cstdio>
#include <iostream>

static Config base_config(const char* db) {
    Config cfg;
    cfg.power.database_path = db;
    cfg.power.startup_readiness_s = 0;
    cfg.power.warning_watts = 500;
    cfg.power.critical_watts = 700;
    return cfg;
}

static protocol::PduSnapshot snapshot(
        std::initializer_list<protocol::OutletState> outlets, float watts = 0.f) {
    protocol::PduSnapshot value;
    value.outlets = outlets;
    value.nominal_volts = 120.f;
    value.estimated_watts = watts;
    value.inlet_amps = watts / value.nominal_volts;
    value.measurements_available = true;
    return value;
}

int main() {
    const char* db_path = "/tmp/command-deck-power-test.db";
    std::remove(db_path);
    auto cfg = base_config(db_path);
    {
        PowerHistoryStore history(cfg.power);
        assert(history.ready());
        const int64_t now = 2'000'000;
        history.record(snapshot({{1, "node", true}}, 100.f), now - 180);
        history.record(snapshot({{1, "node", true}}, 200.f), now - 120);
        history.rollup_and_cleanup(now);
        auto rack = history.analytics(0, 200.f, now);
        assert(rack.current_watts == 200.f);
        assert(rack.average_watts > 140.f && rack.average_watts < 160.f);
        assert(rack.peak_watts == 200.f);
        assert(!history.history(0, now - 3600).empty());
    }

    MetricsStore metrics;
    PduStore pdu;
    ActivityStore activity;
    CommandRouter router(nullptr, activity, [](const std::string&, const std::string&) { return true; });

    cfg.power_groups = {
        {"core", {"storage"}, {}, 0, true, 10, 10},
        {"compute", {"compute-01"}, {"core"}, 100, false, 10, 10},
    };
    protocol::HelloMessage storage;
    storage.host = "storage";
    metrics.update_hello(storage);
    protocol::HelloMessage compute;
    compute.host = "compute-01";
    metrics.update_hello(compute);
    PowerHistoryStore history(cfg.power);
    PowerSequenceEngine engine(cfg, metrics, pdu, router, activity);
    assert(engine.valid());
    assert(engine.start_all(true));
    engine.tick();
    engine.tick();
    engine.tick();
    auto status = engine.status();
    assert(status.succeeded);
    assert(status.steps.size() == 2);
    assert(status.steps[0].group == "core");
    assert(status.steps[1].group == "compute");

    metrics.mark_offline("storage");
    metrics.mark_offline("compute-01");
    metrics.set_pdu_outlet_override("storage", 1);
    metrics.set_pdu_outlet_override("compute-01", 2);
    pdu.update(snapshot({{1, "storage", true}, {2, "compute", true}}));
    PowerSequenceEngine shutdown(cfg, metrics, pdu, router, activity);
    assert(shutdown.start_all(false));
    auto shutdown_status = shutdown.status();
    assert(shutdown_status.steps[0].group == "compute");
    assert(shutdown_status.steps[1].group == "core");
    shutdown.tick();
    pdu.update(snapshot({{1, "storage", true}, {2, "compute", false}}));
    shutdown.tick();
    shutdown.tick();
    pdu.update(snapshot({{1, "storage", false}, {2, "compute", false}}));
    shutdown.tick();
    assert(shutdown.status().succeeded);

    history.record(snapshot({{2, "compute", true}}, 100.f), std::time(nullptr) - 120);
    history.rollup_and_cleanup();
    pdu.update(snapshot({{1, "storage", true}, {2, "compute", false}}, 700.f));
    metrics.update_hello(storage);
    PowerSequenceEngine blocked(cfg, metrics, pdu, router, activity);
    assert(blocked.start_group("compute", true));
    blocked.tick();
    blocked.tick();
    assert(!blocked.status().active);
    assert(!blocked.status().succeeded);
    assert(blocked.status().steps[1].state == SequenceStepState::Failed);

    pdu.mark_poll_failed();
    PowerSequenceEngine disconnected(cfg, metrics, pdu, router, activity);
    assert(disconnected.start_group("compute", false));
    disconnected.tick();
    assert(disconnected.status().steps[0].state == SequenceStepState::Failed);

    auto invalid = cfg;
    invalid.power_groups[0].dependencies = {"missing"};
    PowerSequenceEngine missing(invalid, metrics, pdu, router, activity);
    assert(!missing.valid());
    invalid = cfg;
    invalid.power_groups[0].dependencies = {"compute"};
    PowerSequenceEngine cycle(invalid, metrics, pdu, router, activity);
    assert(!cycle.valid());

    auto shed_cfg = cfg;
    shed_cfg.power.load_shedding_enabled = true;
    shed_cfg.power.critical_hold_s = 0;
    shed_cfg.power.warning_watts = 50;
    shed_cfg.power.critical_watts = 100;
    metrics.mark_offline("compute-01");
    metrics.set_pdu_outlet_override("compute-01", 1);
    pdu.update(snapshot({{1, "compute", true}, {2, "other", true}}, 150.f));
    PowerSequenceEngine shed_engine(shed_cfg, metrics, pdu, router, activity);
    PowerBudgetController budget(shed_cfg, pdu, shed_engine);
    budget.tick();
    budget.tick();
    pdu.update(snapshot({{1, "compute", false}, {2, "other", true}}, 150.f));
    budget.tick();
    assert(budget.shedding());
    assert(budget.shed_groups().count("compute") == 1);
    assert(budget.shed_groups().count("core") == 0);

    std::remove(db_path);
    std::cout << "power core tests passed\n";
}
