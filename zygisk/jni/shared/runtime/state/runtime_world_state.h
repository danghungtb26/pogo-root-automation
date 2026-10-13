#pragma once

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

#include "../../bridge_kotlin/runtime_observation_protocol.h"

namespace pogo_runtime_world_state {

enum class Availability : uint32_t {
    kNotReady = 0U,
    kEmpty = 1U,
    kReady = 2U,
    kStale = 3U,
    kError = 4U,
};

enum class MapCompleteness : uint32_t {
    kUnknown = 0U,
    kComplete = 1U,
    kPartial = 2U,
};

enum DirtySource : uint32_t {
    kPokemonSource = 1U << 0U,
    kFortSource = 1U << 1U,
    kLocationSource = 1U << 2U,
    kInventorySource = 1U << 3U,
    kEncounterSource = 1U << 4U,
    kAllSources = kPokemonSource | kFortSource | kLocationSource |
        kInventorySource | kEncounterSource,
};

struct DirtySources {
    uint64_t pokemon = 0U;
    uint64_t forts = 0U;
    uint64_t location = 0U;
    uint64_t inventory = 0U;
    uint64_t encounter = 0U;
};

struct SectionState {
    Availability availability = Availability::kNotReady;
    uint64_t owner_generation = 0U;
    uint64_t dirty_generation = 0U;
    uint64_t revision = 0U;
    uint64_t observed_at_elapsed_ns = 0U;
    std::string error_code;
};

struct LocationSnapshot {
    SectionState state;
    bool has_coordinate = false;
    double latitude = 0.0;
    double longitude = 0.0;
};

struct MapSnapshot {
    SectionState state;
    MapCompleteness completeness = MapCompleteness::kUnknown;
    pogo_runtime::RuntimeNearbyObservation nearby;
    pogo_runtime::RuntimeFortsObservation forts;
};

struct InventorySnapshot {
    SectionState state;
    pogo_runtime::RuntimeInventoryObservation inventory;
};

struct EncounterSnapshot {
    SectionState state;
    bool has_encounter = false;
    pogo_runtime::RuntimeEncounterObservation encounter;
};

struct Snapshot {
    uint64_t session_generation = 0U;
    uint64_t owner_generation = 0U;
    uint64_t dirty_generation = 0U;
    uint64_t revision = 0U;
    uint64_t committed_at_elapsed_ns = 0U;
    LocationSnapshot location;
    MapSnapshot map;
    InventorySnapshot inventory;
    EncounterSnapshot encounter;
};

Snapshot snapshot();

DirtySources dirty_sources();

uint64_t mark_dirty(uint32_t sources);

void reset(
    uint64_t owner_generation,
    uint64_t observed_at_elapsed_ns
);

uint64_t commit_map(
    uint64_t owner_generation,
    uint64_t dirty_generation,
    uint64_t observed_at_elapsed_ns,
    Availability availability,
    MapCompleteness completeness,
    const pogo_runtime::RuntimeNearbyObservation &nearby,
    const pogo_runtime::RuntimeFortsObservation &forts,
    const char *error_code
);

uint64_t commit_location(
    uint64_t owner_generation,
    uint64_t dirty_generation,
    uint64_t observed_at_elapsed_ns,
    Availability availability,
    bool has_coordinate,
    double latitude,
    double longitude,
    const char *error_code
);

uint64_t commit_inventory(
    uint64_t owner_generation,
    uint64_t dirty_generation,
    uint64_t observed_at_elapsed_ns,
    Availability availability,
    const pogo_runtime::RuntimeInventoryObservation &inventory,
    const char *error_code
);

uint64_t commit_encounter(
    uint64_t owner_generation,
    uint64_t dirty_generation,
    uint64_t observed_at_elapsed_ns,
    Availability availability,
    bool has_encounter,
    const pogo_runtime::RuntimeEncounterObservation &encounter,
    const char *error_code
);

}  // namespace pogo_runtime_world_state
