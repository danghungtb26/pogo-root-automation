#include <cassert>
#include <limits>
#include "modules/catch_spin/auto_fort_navigation.h"

using Kind = pogo_runtime::NavigationKind;
using Snapshot = pogo_runtime::RuntimeCatchSpinObservation;
using Fort = pogo_runtime::RuntimeFortObservation;

Snapshot map() {
    Snapshot value;
    value.nearby_available = true;
    value.nearby.is_complete = true;
    value.forts_available = true;
    value.has_player_position = true;
    value.player_latitude = 10;
    value.player_longitude = 106;
    value.forts.forts = {Fort{"first", 0, 10, 106.001, true},
        Fort{"second", 0, 10, 106.002, true}, Fort{"gym", 1, 10, 106.0001, true}};
    return value;
}

int main() {
    RuntimeAutoFortNavigation navigation;
    auto snapshot = map();
    assert(navigation.update(snapshot).back().fort_id == "first");
    // Repeated snapshots renew the same destination, not a new target.
    assert(navigation.update(snapshot).back().fort_id == "first");
    assert(navigation.pause("catch pending").kind == Kind::kStop);
    snapshot.nearby.spawns.push_back({1, 1, 10, 106});
    assert(navigation.update(snapshot).back().kind == Kind::kStop);
    snapshot.nearby.spawns.clear(); // A despawn must not latch a catch state.
    assert(navigation.update(snapshot).back().fort_id == "first");
    snapshot.nearby.is_complete = false;
    assert(navigation.update(snapshot).back().kind == Kind::kStop);
    snapshot = map();
    snapshot.forts_available = false;
    assert(navigation.update(snapshot).back().kind == Kind::kStop);
    snapshot = map();
    snapshot.has_player_position = false;
    assert(navigation.update(snapshot).back().kind == Kind::kStop);
    snapshot = map();
    snapshot.player_latitude = std::numeric_limits<double>::quiet_NaN();
    assert(navigation.update(snapshot).back().kind == Kind::kStop);
    snapshot = map();
    snapshot.forts.forts[0].spin_available = false;
    assert(navigation.update(snapshot).back().fort_id == "second");
    snapshot.forts.forts.clear();
    assert(navigation.update(snapshot).back().kind == Kind::kStop);
    navigation.reset();
    snapshot = map();
    navigation.update(snapshot);
    snapshot.player_longitude = 106.001;
    const auto arrival = navigation.update(snapshot);
    assert(arrival.size() == 2);
    assert(arrival.front().kind == Kind::kArrived && arrival.front().fort_id == "first");
    assert(arrival.back().kind == Kind::kWalk && arrival.back().fort_id == "second");
    assert(navigation.update(snapshot).size() == 1); // arrival reported once
    navigation.reset();
    snapshot = map();
    snapshot.forts.forts.resize(1);
    navigation.update(snapshot);
    snapshot.player_longitude = 106.001;
    assert(navigation.update(snapshot).back().kind == Kind::kStop);
    navigation.reset(); // next game session must not inherit last-arrived state
    assert(navigation.update(map()).back().fort_id == "first");

    std::vector<uint8_t> bytes;
    assert(pogo_runtime::encode_runtime_navigation(
        {Kind::kWalk, "first", 10, 106.001, ""}, 100, 200, &bytes));
    assert(bytes[7] == 11 && bytes[23] == 1); // observation type and WALK kind
    assert(bytes[16] == 'P' && bytes[19] == 'W');
    assert(!pogo_runtime::encode_runtime_navigation({Kind::kWalk, "", 10, 106, ""}, 1, 1, &bytes));
    assert(!pogo_runtime::encode_runtime_navigation({Kind::kWalk, "x", 91, 106, ""}, 1, 1, &bytes));
}
