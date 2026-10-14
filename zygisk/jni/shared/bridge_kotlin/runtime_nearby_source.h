#pragma once

#include <cstdint>

namespace pogo_runtime {

enum class RuntimeNearbySourceType : uint32_t {
    kUnknown = 0U,
    kWild = 1U,
    kIncense = 2U,
    kTroyDisk = 3U,
    kDailyEncounter = 4U,
    kPhotobomb = 5U,
    kIncident = 6U,
    kPokestopSpawn = 7U,
    kRoute = 8U,
    kRaid = 9U,
    kStation = 10U,
};

inline const char *runtime_nearby_source_name(RuntimeNearbySourceType source) {
    switch (source) {
        case RuntimeNearbySourceType::kWild: return "wild";
        case RuntimeNearbySourceType::kIncense: return "incense";
        case RuntimeNearbySourceType::kTroyDisk: return "troy_disk";
        case RuntimeNearbySourceType::kDailyEncounter: return "daily_encounter";
        case RuntimeNearbySourceType::kPhotobomb: return "photobomb";
        case RuntimeNearbySourceType::kIncident: return "incident";
        case RuntimeNearbySourceType::kPokestopSpawn: return "pokestop_spawn";
        case RuntimeNearbySourceType::kRoute: return "route";
        case RuntimeNearbySourceType::kRaid: return "raid";
        case RuntimeNearbySourceType::kStation: return "station";
        case RuntimeNearbySourceType::kUnknown: return "unknown";
    }
    return "unknown";
}

inline bool runtime_nearby_source_known(RuntimeNearbySourceType source) {
    return source >= RuntimeNearbySourceType::kWild &&
        source <= RuntimeNearbySourceType::kStation;
}

inline bool runtime_nearby_source_direct_catch_eligible(RuntimeNearbySourceType source) {
    return source == RuntimeNearbySourceType::kWild;
}

}  // namespace pogo_runtime
