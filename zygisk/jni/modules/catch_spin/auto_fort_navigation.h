#pragma once
#include <algorithm>
#include "../../shared/bridge_kotlin/runtime_navigation_protocol.h"
#include "../../shared/bridge_kotlin/runtime_catch_spin_protocol.h"

// All fort selection and pause/resume policy lives here. Android only executes
// the resulting destination using its mock-location service.
class RuntimeAutoFortNavigation {
public:
    using Command = pogo_runtime::RuntimeNavigation;
    using Kind = pogo_runtime::NavigationKind;
    using Fort = pogo_runtime::RuntimeFortObservation;

    Command pause(const std::string &reason) const {
        return {Kind::kStop, "", 0, 0, reason};
    }
    Command reset() {
        target_ = Fort{};
        last_arrived_.clear();
        return pause("automation reset");
    }

    std::vector<Command> update(const pogo_runtime::RuntimeCatchSpinObservation &snapshot) {
        if (!snapshot.nearby_available || !snapshot.nearby.is_complete ||
            !snapshot.forts_available || !snapshot.has_player_position ||
            !pogo_runtime::valid_navigation_coordinate(snapshot.player_latitude, snapshot.player_longitude)) {
            return {pause("map unavailable or incomplete")};
        }
        // Suppress duplicate arrival only while standing at the same available
        // fort. A cooldown or moving away starts a new visit; this is not a blacklist.
        if (!last_arrived_.empty()) {
            const auto previous = std::find_if(snapshot.forts.forts.begin(), snapshot.forts.forts.end(),
                [this](const Fort &fort) { return fort.fort_id == last_arrived_; });
            if (previous == snapshot.forts.forts.end() || !eligible(*previous) ||
                distance(snapshot, *previous) > kArrivalMeters) last_arrived_.clear();
        }
        if (!snapshot.nearby.spawns.empty()) return {pause("pokemon nearby")};
        std::vector<Command> commands;
        if (!target_.fort_id.empty() && distance(snapshot, target_) <= kArrivalMeters) {
            last_arrived_ = target_.fort_id;
            commands.push_back({Kind::kArrived, target_.fort_id, target_.latitude, target_.longitude, ""});
            target_ = Fort{};
        }
        if (!target_.fort_id.empty()) {
            const auto found = std::find_if(snapshot.forts.forts.begin(), snapshot.forts.forts.end(),
                [this](const Fort &fort) { return fort.fort_id == target_.fort_id && eligible(fort); });
            if (found == snapshot.forts.forts.end()) target_ = Fort{};
            else target_ = *found;
        }
        if (target_.fort_id.empty()) {
            const Fort *nearest = nullptr;
            const Fort *away = nullptr;
            for (const auto &fort : snapshot.forts.forts) {
                if (!eligible(fort) || fort.fort_id == last_arrived_) continue;
                if (nearer(snapshot, fort, nearest)) nearest = &fort;
                if (distance(snapshot, fort) > kArrivalMeters && nearer(snapshot, fort, away)) away = &fort;
            }
            const Fort *selected = away != nullptr ? away : nearest;
            if (selected != nullptr) target_ = *selected;
        }
        commands.push_back(target_.fort_id.empty() ? pause("no available PokéStop")
            : Command{Kind::kWalk, target_.fort_id, target_.latitude, target_.longitude, ""});
        return commands;
    }

private:
    static constexpr double kArrivalMeters = 12.0;
    Fort target_;
    std::string last_arrived_;

    static bool eligible(const Fort &fort) {
        return !fort.fort_id.empty() && fort.type == 0 && fort.spin_available &&
            pogo_runtime::valid_navigation_coordinate(fort.latitude, fort.longitude);
    }
    static double distance(const pogo_runtime::RuntimeCatchSpinObservation &snapshot, const Fort &fort) {
        constexpr double radians = 0.017453292519943295;
        const double a = snapshot.player_latitude * radians;
        const double b = fort.latitude * radians;
        const double dlat = (fort.latitude - snapshot.player_latitude) * radians;
        const double dlon = (fort.longitude - snapshot.player_longitude) * radians;
        const double h = std::sin(dlat / 2) * std::sin(dlat / 2) +
            std::cos(a) * std::cos(b) * std::sin(dlon / 2) * std::sin(dlon / 2);
        return 6371000.0 * 2 * std::asin(std::sqrt(std::min(1.0, std::max(0.0, h))));
    }
    static bool nearer(const pogo_runtime::RuntimeCatchSpinObservation &snapshot,
        const Fort &fort, const Fort *other) {
        if (other == nullptr) return true;
        const double d = distance(snapshot, fort), previous = distance(snapshot, *other);
        return d < previous || (d == previous && fort.fort_id < other->fort_id);
    }
};
