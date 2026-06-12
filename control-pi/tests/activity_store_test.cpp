#include "activity_store.h"
#include <cassert>

int main() {
    ActivityStore store;

    protocol::CommandMessage first;
    first.id = "host_reboot_1";
    first.action = "reboot";
    store.begin(first, "host");

    protocol::CommandMessage second;
    second.id = "host_service_2";
    second.action = "service";
    store.begin(second, "host", "nginx restart");

    store.complete({"cmd_result", second.id, false, "restart failed"});

    auto recent = store.snapshot();
    assert(recent.size() == 2);
    assert(recent[0].id == second.id);
    assert(recent[0].state == ActivityState::Failed);
    assert(recent[0].output == "restart failed");
    assert(recent[1].state == ActivityState::Pending);

    auto host = store.for_host("host", 1);
    assert(host.size() == 1);
    assert(host[0].id == second.id);
    return 0;
}
