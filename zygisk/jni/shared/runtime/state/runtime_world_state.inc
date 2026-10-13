namespace pogo_runtime_world_state {
namespace {

struct Store {
    std::mutex mutex;
    Snapshot current;
    uint64_t next_revision = 1U;
    uint64_t next_session_generation = 1U;
    uint64_t next_dirty_generation = 1U;
    DirtySources dirty;
};

Store g_store;

SectionState section_state(
    Availability availability,
    uint64_t owner_generation,
    uint64_t dirty_generation,
    uint64_t revision,
    uint64_t observed_at_elapsed_ns,
    const char *error_code
) {
    SectionState state;
    state.availability = availability;
    state.owner_generation = owner_generation;
    state.dirty_generation = dirty_generation;
    state.revision = revision;
    state.observed_at_elapsed_ns = observed_at_elapsed_ns;
    if (error_code != nullptr) state.error_code = error_code;
    return state;
}

bool accepts_owner(const Snapshot &current, uint64_t owner_generation) {
    return current.session_generation != 0U && owner_generation != 0U &&
        owner_generation == current.owner_generation;
}

uint64_t next_revision_locked(Store &store, uint64_t observed_at_elapsed_ns) {
    const uint64_t revision = store.next_revision++;
    store.current.revision = revision;
    store.current.committed_at_elapsed_ns = observed_at_elapsed_ns;
    return revision;
}

}  // namespace

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_store.mutex);
    return g_store.current;
}

DirtySources dirty_sources() {
    std::lock_guard<std::mutex> lock(g_store.mutex);
    return g_store.dirty;
}

uint64_t mark_dirty(uint32_t sources) {
    if (sources == 0U) return 0U;
    std::lock_guard<std::mutex> lock(g_store.mutex);
    const uint64_t generation = g_store.next_dirty_generation++;
    if ((sources & kPokemonSource) != 0U) g_store.dirty.pokemon = generation;
    if ((sources & kFortSource) != 0U) g_store.dirty.forts = generation;
    if ((sources & kLocationSource) != 0U) g_store.dirty.location = generation;
    if ((sources & kInventorySource) != 0U) g_store.dirty.inventory = generation;
    if ((sources & kEncounterSource) != 0U) g_store.dirty.encounter = generation;
    g_store.current.dirty_generation = generation;
    return generation;
}

void reset(uint64_t owner_generation, uint64_t observed_at_elapsed_ns) {
    std::lock_guard<std::mutex> lock(g_store.mutex);
    const uint64_t dirty_generation = g_store.next_dirty_generation++;
    g_store.dirty = {
        dirty_generation,
        dirty_generation,
        dirty_generation,
        dirty_generation,
        dirty_generation,
    };
    Snapshot replacement;
    replacement.session_generation = g_store.next_session_generation++;
    replacement.owner_generation = owner_generation;
    replacement.dirty_generation = dirty_generation;
    g_store.current = std::move(replacement);
    const uint64_t revision = next_revision_locked(g_store, observed_at_elapsed_ns);
    g_store.current.location.state.revision = revision;
    g_store.current.map.state.revision = revision;
    g_store.current.inventory.state.revision = revision;
    g_store.current.encounter.state.revision = revision;
}

uint64_t commit_map(
    uint64_t owner_generation,
    uint64_t dirty_generation,
    uint64_t observed_at_elapsed_ns,
    Availability availability,
    MapCompleteness completeness,
    const pogo_runtime::RuntimeNearbyObservation &nearby,
    const pogo_runtime::RuntimeFortsObservation &forts,
    const char *error_code
) {
    std::lock_guard<std::mutex> lock(g_store.mutex);
    if (!accepts_owner(g_store.current, owner_generation)) return 0U;
    const uint64_t revision = next_revision_locked(g_store, observed_at_elapsed_ns);
    g_store.current.dirty_generation = std::max(
        g_store.current.dirty_generation, dirty_generation
    );
    MapSnapshot replacement;
    replacement.state = section_state(
        availability, owner_generation, dirty_generation, revision,
        observed_at_elapsed_ns, error_code
    );
    replacement.completeness = completeness;
    replacement.nearby = nearby;
    replacement.forts = forts;
    g_store.current.map = std::move(replacement);
    return revision;
}

uint64_t commit_location(
    uint64_t owner_generation,
    uint64_t dirty_generation,
    uint64_t observed_at_elapsed_ns,
    Availability availability,
    bool has_coordinate,
    double latitude,
    double longitude,
    const char *error_code
) {
    std::lock_guard<std::mutex> lock(g_store.mutex);
    if (!accepts_owner(g_store.current, owner_generation)) return 0U;
    const uint64_t revision = next_revision_locked(g_store, observed_at_elapsed_ns);
    g_store.current.dirty_generation = std::max(
        g_store.current.dirty_generation, dirty_generation
    );
    LocationSnapshot replacement;
    replacement.state = section_state(
        availability, owner_generation, dirty_generation, revision,
        observed_at_elapsed_ns, error_code
    );
    replacement.has_coordinate = has_coordinate;
    replacement.latitude = latitude;
    replacement.longitude = longitude;
    g_store.current.location = std::move(replacement);
    return revision;
}

uint64_t commit_inventory(
    uint64_t owner_generation,
    uint64_t dirty_generation,
    uint64_t observed_at_elapsed_ns,
    Availability availability,
    const pogo_runtime::RuntimeInventoryObservation &inventory,
    const char *error_code
) {
    std::lock_guard<std::mutex> lock(g_store.mutex);
    if (!accepts_owner(g_store.current, owner_generation)) return 0U;
    const uint64_t revision = next_revision_locked(g_store, observed_at_elapsed_ns);
    g_store.current.dirty_generation = std::max(
        g_store.current.dirty_generation, dirty_generation
    );
    InventorySnapshot replacement;
    replacement.state = section_state(
        availability, owner_generation, dirty_generation, revision,
        observed_at_elapsed_ns, error_code
    );
    replacement.inventory = inventory;
    g_store.current.inventory = std::move(replacement);
    return revision;
}

uint64_t commit_encounter(
    uint64_t owner_generation,
    uint64_t dirty_generation,
    uint64_t observed_at_elapsed_ns,
    Availability availability,
    bool has_encounter,
    const pogo_runtime::RuntimeEncounterObservation &encounter,
    const char *error_code
) {
    std::lock_guard<std::mutex> lock(g_store.mutex);
    if (!accepts_owner(g_store.current, owner_generation)) return 0U;
    const uint64_t revision = next_revision_locked(g_store, observed_at_elapsed_ns);
    g_store.current.dirty_generation = std::max(
        g_store.current.dirty_generation, dirty_generation
    );
    EncounterSnapshot replacement;
    replacement.state = section_state(
        availability, owner_generation, dirty_generation, revision,
        observed_at_elapsed_ns, error_code
    );
    replacement.has_encounter = has_encounter;
    replacement.encounter = encounter;
    g_store.current.encounter = std::move(replacement);
    return revision;
}

}  // namespace pogo_runtime_world_state
