bool read_runtime_managed_ascii_string(
    void *managed_string,
    std::string *value
) {
    if (value == nullptr || !is_probable_managed_pointer(managed_string)) return false;
    constexpr size_t kStringLengthOffset = sizeof(void *) * 2U;
    constexpr size_t kStringCharsOffset = kStringLengthOffset + sizeof(int32_t);
    int32_t length = 0;
    memcpy(&length, static_cast<const uint8_t *>(managed_string) + kStringLengthOffset, sizeof(length));
    if (length <= 0 || length > 1024) return false;
    const auto *characters = reinterpret_cast<const uint16_t *>(
        static_cast<const uint8_t *>(managed_string) + kStringCharsOffset
    );
    value->clear();
    value->reserve(static_cast<size_t>(length));
    for (int32_t index = 0; index < length; ++index) {
        const uint16_t character = characters[index];
        if (character > 0x7fU) return false;
        value->push_back(static_cast<char>(character));
    }
    return !value->empty();
}

bool read_runtime_boxed_lat_lng(
    const Il2CppApi &api,
    void *boxed,
    double *latitude,
    double *longitude
) {
    if (latitude == nullptr || longitude == nullptr || api.object_unbox == nullptr ||
        !is_probable_managed_pointer(boxed)) return false;
    void *unboxed = api.object_unbox(boxed);
    if (!is_probable_managed_pointer(unboxed)) return false;
    memcpy(latitude, unboxed, sizeof(*latitude));
    memcpy(longitude, static_cast<const uint8_t *>(unboxed) + sizeof(*latitude), sizeof(*longitude));
    return std::isfinite(*latitude) && *latitude >= -90.0 && *latitude <= 90.0 &&
        std::isfinite(*longitude) && *longitude >= -180.0 && *longitude <= 180.0;
}

bool read_runtime_boxed_double(const Il2CppApi &api, void *boxed, double *value) {
    if (value == nullptr || api.object_unbox == nullptr ||
        !is_probable_managed_pointer(boxed)) return false;
    void *unboxed = api.object_unbox(boxed);
    if (!is_probable_managed_pointer(unboxed)) return false;
    memcpy(value, unboxed, sizeof(*value));
    return std::isfinite(*value);
}

// Reads the current player position via ILocationProvider.get_CurrentLocation()
// -> Niantic.Platform.Ditto.Geo.Location (value struct) -> LatitudeDeg/LongitudeDeg.
// get_CurrentLocation returns the struct by value, which il2cpp_runtime_invoke
// boxes; the Location getters are value-type instance methods, so they are
// invoked with the unboxed interior pointer as `this`. Needs device verification.
bool read_runtime_player_position(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    double *latitude,
    double *longitude
) {
    if (latitude == nullptr || longitude == nullptr || !binding.location_read_verified ||
        binding.location_provider == nullptr || binding.location_get_current == nullptr ||
        binding.location_get_latitude == nullptr || binding.location_get_longitude == nullptr ||
        api.runtime_invoke == nullptr || api.object_unbox == nullptr) return false;
    void *exception = nullptr;
    void *boxed_location = api.runtime_invoke(
        binding.location_get_current, binding.location_provider, nullptr, &exception
    );
    if (exception != nullptr || !is_probable_managed_pointer(boxed_location)) return false;
    void *location_value = api.object_unbox(boxed_location);
    if (!is_probable_managed_pointer(location_value)) return false;
    exception = nullptr;
    void *boxed_lat = api.runtime_invoke(
        binding.location_get_latitude, location_value, nullptr, &exception
    );
    if (exception != nullptr || !read_runtime_boxed_double(api, boxed_lat, latitude)) return false;
    exception = nullptr;
    void *boxed_lng = api.runtime_invoke(
        binding.location_get_longitude, location_value, nullptr, &exception
    );
    if (exception != nullptr || !read_runtime_boxed_double(api, boxed_lng, longitude)) return false;
    return *latitude >= -90.0 && *latitude <= 90.0 &&
        *longitude >= -180.0 && *longitude <= 180.0;
}

// Reads the count of a single item via IItemBag.GetItemCount(Item). Returns the
// count, or -1 when the inventory binding is unavailable or the read fails (so
// callers can distinguish "zero" from "unknown").
int32_t read_runtime_item_count(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    int32_t item_id
) {
    if (!binding.inventory_read_verified || binding.item_bag == nullptr ||
        binding.item_bag_get_item_count == nullptr || api.runtime_invoke == nullptr ||
        api.object_unbox == nullptr) return -1;
    int32_t item_arg = item_id;
    void *args[] = {&item_arg};
    void *exception = nullptr;
    void *boxed = api.runtime_invoke(
        binding.item_bag_get_item_count, binding.item_bag, args, &exception
    );
    if (exception != nullptr || !is_probable_managed_pointer(boxed)) return -1;
    void *unboxed = api.object_unbox(boxed);
    if (!is_probable_managed_pointer(unboxed)) return -1;
    int32_t count = 0;
    memcpy(&count, unboxed, sizeof(count));
    return count < 0 ? -1 : count;
}

bool read_runtime_map_pokestop(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    void *pokestop,
    pogo_runtime::RuntimeFortObservation *fort
) {
    if (fort == nullptr || !runtime_object_is_exact_class(
            api, pokestop, "Niantic.Holoholo.Pokestop", "MapPokestop")) return false;
    void *managed_id = invoke_runtime_method(
        api, binding.map_pokestop_get_id, pokestop, nullptr, "MapPokestop.get_Id"
    );
    if (!read_runtime_managed_ascii_string(managed_id, &fort->fort_id)) return false;
    void *boxed_location = invoke_runtime_method(
        api, binding.map_pokestop_get_location, pokestop, nullptr, "MapPokestop.get_Location"
    );
    if (!read_runtime_boxed_lat_lng(
            api, boxed_location, &fort->latitude, &fort->longitude)) return false;
    void *boxed_cooldown = invoke_runtime_method(
        api,
        binding.map_pokestop_get_is_cooling_down,
        pokestop,
        nullptr,
        "MapPokestop.get_IsCoolingDown"
    );
    bool cooling_down = false;
    if (!read_boxed_bool(api, boxed_cooldown, &cooling_down)) return false;
    fort->type = 0;
    fort->spin_available = !cooling_down;
    return true;
}

bool read_runtime_map_forts(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    pogo_runtime::RuntimeFortsObservation *observation
) {
    if (observation == nullptr || !binding.forts_read_verified ||
        binding.map_place_directory_service == nullptr ||
        binding.map_place_directory_pokestops == nullptr) return false;
    observation->forts.clear();
    std::vector<MapDictionaryEntry> entries;
    if (!enumerate_map_dictionary(
            api, binding.map_place_directory_pokestops, &entries)) return false;
    for (const MapDictionaryEntry &entry : entries) {
        void *pokestop = entry.value;
        pogo_runtime::RuntimeFortObservation fort{};
        if (!read_runtime_map_pokestop(api, binding, pokestop, &fort)) continue;
        const bool duplicate = std::any_of(
            observation->forts.begin(),
            observation->forts.end(),
            [&fort](const auto &existing) { return existing.fort_id == fort.fort_id; }
        );
        if (!duplicate) observation->forts.push_back(std::move(fort));
    }
    return true;
}
