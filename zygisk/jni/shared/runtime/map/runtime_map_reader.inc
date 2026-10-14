namespace { void maintain_runtime_map_after_actions(ProbeContext &context); }

bool read_runtime_map_snapshot_on_main_thread(
    ProbeContext &context,
    pogo_runtime::RuntimeNearbyObservation *observation,
    pogo_runtime::RuntimeFortsObservation *forts
) {
    maintain_runtime_map_after_actions(context);
    // Keep the collection walk staged until each IL2CPP call has passed the
    // live build smoke test. Stage 3 uses dictionary keys and never treats the
    // broken value-collection Current return as a managed object.
    constexpr int kMapReaderStage = 5;
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    __android_log_print(
        binding.map_entity_read_verified || binding.forts_read_verified
            ? ANDROID_LOG_INFO
            : ANDROID_LOG_WARN,
        kLogTag,
        "runtime map scan begin stage=%d map_verified=%d map_service=%p "
        "get_cells=%p get_all_pokemon=%p forts_verified=%d fort_directory=%p pokestops=%p",
        kMapReaderStage,
        binding.map_entity_read_verified ? 1 : 0,
        binding.map_entity_service,
        binding.map_entity_get_cells,
        binding.map_entity_get_all_pokemon,
        binding.forts_read_verified ? 1 : 0,
        binding.map_place_directory_service,
        binding.map_place_directory_pokestops
    );
    if (kMapReaderStage == 0) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map scan rejected reason=reader_stage_disabled"
        );
        return false;
    }
    if (observation == nullptr || forts == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map scan rejected reason=output_pointer_missing observation=%p forts=%p",
            observation,
            forts
        );
        return false;
    }
    if (!has_core_il2cpp_api(context.il2cpp_api)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map scan rejected reason=il2cpp_api_unavailable"
        );
        return false;
    }

    // Fort reading does not depend on the map-entity cell traversal. When the
    // pokemon cell reader is unavailable but the fort directory is verified,
    // still publish forts so fort/spin observation keeps working on its own.
    if (!binding.map_entity_read_verified ||
        binding.map_entity_service == nullptr ||
        binding.map_entity_get_cells == nullptr ||
        binding.map_entity_get_all_pokemon == nullptr) {
        if (!binding.forts_read_verified) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime map scan rejected reason=map_and_fort_bindings_unavailable "
                "map_verified=%d forts_verified=%d",
                binding.map_entity_read_verified ? 1 : 0,
                binding.forts_read_verified ? 1 : 0
            );
            return false;
        }
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime map scan mode=fort_only reason=map_entity_binding_unavailable"
        );
        observation->spawns.clear();
        observation->is_complete = true;
        forts->forts.clear();
        observation->has_player_position = read_runtime_player_position(
            context.il2cpp_api, binding,
            &observation->player_latitude, &observation->player_longitude
        );
        const bool forts_ok = read_runtime_map_forts(context.il2cpp_api, binding, forts);
        __android_log_print(
            forts_ok ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
            kLogTag,
            "runtime map scan complete mode=fort_only success=%d nearby=0 forts=%zu player=%d",
            forts_ok ? 1 : 0,
            forts->forts.size(),
            observation->has_player_position ? 1 : 0
        );
        return forts_ok;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime map scan mode=nearby_and_forts service=%p",
        binding.map_entity_service
    );
    void *cells = invoke_runtime_method(
        context.il2cpp_api,
        binding.map_entity_get_cells,
        binding.map_entity_service,
        nullptr,
        "map.GetCells"
    );
    if (cells == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map scan rejected reason=get_cells_returned_null service=%p",
            binding.map_entity_service
        );
        return false;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime map scan cells acquired=%p",
        cells
    );
    if (kMapReaderStage == 1) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime map scan complete stage=1 success=1 cells_only=1"
        );
        return true;
    }
    observation->spawns.clear();
    observation->is_complete = true;
    forts->forts.clear();
    if (kMapReaderStage == 2) {
        const size_t collection_count = count_map_collection(context.il2cpp_api, cells);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime map scan complete stage=2 success=1 cells=%zu",
            collection_count
        );
        return true;
    }
    if (kMapReaderStage == 3) {
        std::vector<uint64_t> cell_keys;
        if (!enumerate_map_keys(context.il2cpp_api, binding.map_entity_cells, &cell_keys)) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime map scan rejected reason=cell_keys_enumeration_failed dictionary=%p",
                binding.map_entity_cells
            );
            return false;
        }
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime map scan complete stage=3 success=1 cell_keys=%zu",
            cell_keys.size()
        );
        return true;
    }
    // Forts and the player position are independent of the pokemon-cell walk
    // (their bindings are captured separately). Read them FIRST so a failure in
    // the fragile MapEntityCell traversal below cannot suppress them — the walk
    // returns false on a bad cell, and the caller keys nearby availability off
    // that return value while still consuming the forts/player it populated here.
    if (binding.forts_read_verified) {
        const bool forts_ok = read_runtime_map_forts(context.il2cpp_api, binding, forts);
        __android_log_print(
            forts_ok ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
            kLogTag,
            "runtime map scan forts success=%d count=%zu",
            forts_ok ? 1 : 0,
            forts->forts.size()
        );
    } else {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map scan forts skipped reason=binding_unverified"
        );
    }
    observation->has_player_position = read_runtime_player_position(
        context.il2cpp_api, binding,
        &observation->player_latitude, &observation->player_longitude
    );
    __android_log_print(
        observation->has_player_position ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime map scan player_position available=%d latitude=%.7f longitude=%.7f",
        observation->has_player_position ? 1 : 0,
        observation->player_latitude,
        observation->player_longitude
    );

    std::vector<MapDictionaryEntry> cell_entries;
    if (!enumerate_map_dictionary(
            context.il2cpp_api, binding.map_entity_cells, &cell_entries)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map scan rejected reason=cell_dictionary_enumeration_failed dictionary=%p "
            "forts=%zu player=%d",
            binding.map_entity_cells,
            forts->forts.size(),
            observation->has_player_position ? 1 : 0
        );
        return false;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime map scan cells enumerated count=%zu",
        cell_entries.size()
    );
    RuntimeMapSourceReadStats source_stats;
    for (const MapDictionaryEntry &cell_entry : cell_entries) {
        if (observation->spawns.size() >= 512U) {
            observation->is_complete = false;
            source_stats.capped = true;
            break;
        }
        void *cell = cell_entry.value;
        if (cell == nullptr || !read_runtime_map_cell_sources(
                context.il2cpp_api,
                binding.map_entity_get_all_pokemon,
                cell,
                &observation->spawns,
                &source_stats
            )) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime map scan partial reason=cell_source_read_failed key=%llu cell=%p "
                "cells=%zu pokemon=%zu forts=%zu",
                static_cast<unsigned long long>(cell_entry.key),
                cell,
                cell_entries.size(),
                observation->spawns.size(),
                forts->forts.size()
            );
            ++source_stats.cell_failures;
        }
    }
    if (source_stats.unknown != 0U || source_stats.unreadable != 0U ||
        source_stats.cell_failures != 0U || source_stats.capped) {
        observation->is_complete = false;
    }
    const bool valid = pogo_runtime::valid_runtime_nearby_observation(*observation);
    __android_log_print(
        valid ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime map scan complete success=%d cells=%zu pokemon=%zu visited=%zu accepted=%zu "
        "duplicate=%zu unknown=%zu unreadable=%zu cell_failures=%zu capped=%d "
        "forts=%zu complete=%d player=%d",
        valid ? 1 : 0,
        cell_entries.size(),
        observation->spawns.size(),
        source_stats.visited,
        source_stats.accepted,
        source_stats.duplicate,
        source_stats.unknown,
        source_stats.unreadable,
        source_stats.cell_failures,
        source_stats.capped ? 1 : 0,
        forts->forts.size(),
        observation->is_complete ? 1 : 0,
        observation->has_player_position ? 1 : 0
    );
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime map sources wild=%zu incense=%zu troy_disk=%zu daily=%zu photobomb=%zu "
        "incident=%zu pokestop=%zu route=%zu raid=%zu station=%zu",
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kWild)],
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kIncense)],
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kTroyDisk)],
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kDailyEncounter)],
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kPhotobomb)],
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kIncident)],
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kPokestopSpawn)],
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kRoute)],
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kRaid)],
        source_stats.accepted_by_source[
            static_cast<uint32_t>(pogo_runtime::RuntimeNearbySourceType::kStation)]
    );
    return valid;
}
