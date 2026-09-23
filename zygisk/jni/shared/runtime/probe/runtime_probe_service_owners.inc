    struct ServiceCandidate {
        const char *name_space;
        const char *name;
        const char *label;
        const char *expected_class;
    };
    static const ServiceCandidate services[] = {
        {"Niantic.Holoholo", "IItemBag", "IItemBag", "ItemBagImpl"},
        {"Niantic.Holoholo", "IPlayerService", "IPlayerService", "PlayerService"},
        {"Niantic.Holoholo.UI.Items", "IItemInventoryService", "IItemInventoryService", "ItemInventoryService"},
        {"Niantic.Holoholo", "IPokemonBag", "IPokemonBag", "PokemonBagImpl"},
        {"", "IGameMasterData", "IGameMasterData", "GameMasterData"},
        {"Niantic.Holoholo", "ILocationProvider", "ILocationProvider", "NativeLocationProvider"},
        {"Niantic.Holoholo.Encounter", "IEncounterPokemon", "IEncounterPokemon", "EncounterPokemon"},
        {"Niantic.Holoholo.Encounter", "IEncounterState", "IEncounterState", "EncounterState"},
        {"Niantic.Holoholo.Encounter", "IEncounterInteractionState", "IEncounterInteractionState", "EncounterInteractionState"},
        {"Niantic.Holoholo.Encounter", "IEncounterGuiController", "IEncounterGuiController", "EncounterGuiController"},
        {"Niantic.Holoholo.Encounter", "IEncounterSnapshotGuiController", "IEncounterSnapshotGuiController", "EncounterSnapshotGuiController"},
        {"Niantic.Holoholo.Services", "IPokeballService", "IPokeballService", "PokeballService"},
        {"Niantic.Holoholo.NearbyPokemon", "INearbyPokemonService", "INearbyPokemonService", "NearbyPokemonService"},
        {"Niantic.Holoholo.Map", "IMapSceneViewService", "IMapSceneViewService", "MapSceneViewService"},
        {"Niantic.Holoholo.Services.DynamicTappables", "IDynamicTappablesService", "IDynamicTappablesService", "DynamicTappablesService"},
        {"Niantic.Holoholo.UI", "IAwardItemBubbleService", "IAwardItemBubbleService", "AwardItemBubbleService"},
    };
    for (const ServiceCandidate &service : services) {
        Il2CppClass *service_class = find_runtime_class(
            api,
            assemblies,
            assembly_count,
            service.name_space,
            service.name,
            nullptr
        );
        if (service_class == nullptr) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: service class missing %s.%s",
                service.name_space,
                service.name
            );
            continue;
        }
        const Il2CppType *service_type = api.class_get_type(service_class);
        void *type_object = service_type == nullptr ? nullptr : api.type_get_object(service_type);
        if (type_object == nullptr) {
            log_runtime_object(api, nullptr, service.label, service.expected_class);
            continue;
        }
        void *arguments[] = {type_object};
        void *owner = invoke_runtime_method(api, try_resolve, container, arguments, service.label);
        if (owner == nullptr && scene_container != nullptr && scene_container != container) {
            owner = invoke_runtime_method(api, try_resolve, scene_container, arguments, service.label);
        }
        log_runtime_object(api, owner, service.label, service.expected_class);
        if (strcmp(service.label, "IItemBag") == 0 && owner != nullptr &&
            api.object_get_class != nullptr && api.class_get_name != nullptr) {
            Il2CppClass *owner_class = api.object_get_class(owner);
            const char *owner_class_name = owner_class == nullptr
                ? nullptr
                : api.class_get_name(owner_class);
            const void *use_item_method = find_runtime_method_with_two_parameters(
                api,
                owner_class,
                "UseItemOnPokemon",
                "Holoholo.Rpc.Item",
                "System.UInt64"
            );
            const bool is_item_bag_impl = owner_class_name != nullptr &&
                strcmp(owner_class_name, "ItemBagImpl") == 0;
            if (is_item_bag_impl && use_item_method != nullptr) {
                binding->item_bag = owner;
                binding->use_item_on_pokemon = use_item_method;
                binding->use_berry_verified = true;
            }
            if (is_item_bag_impl) {
                // Inventory read: GetItemCount(Holoholo.Rpc.Item) -> int. Ensure
                // item_bag is set even when the berry path is unavailable.
                const void *get_item_count = find_runtime_method_with_parameter(
                    api, owner_class, "GetItemCount", "Holoholo.Rpc.Item"
                );
                if (get_item_count != nullptr) {
                    binding->item_bag = owner;
                    binding->item_bag_get_item_count = get_item_count;
                    binding->inventory_read_verified = true;
                }
                const void *get_total_items = api.class_get_method_from_name == nullptr
                    ? nullptr
                    : api.class_get_method_from_name(owner_class, "GetTotalItems", 0);
                if (get_total_items != nullptr) {
                    binding->item_bag = owner;
                    binding->item_bag_get_total_items = get_total_items;
                }
                __android_log_print(
                    binding->inventory_read_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                    kLogTag,
                    "managed diagnostic: inventory read verified=%d item_bag=%p get_item_count=%p "
                    "get_total_items=%p",
                    binding->inventory_read_verified ? 1 : 0,
                    owner,
                    get_item_count,
                    get_total_items
                );
                void *inventory_cache = nullptr;
                const bool cache_field_read = read_runtime_field(
                    api, owner, "cwch", &inventory_cache
                );
                const void *cache_timestamp = nullptr;
                const void *cache_update = nullptr;
                if (cache_field_read && is_probable_managed_pointer(inventory_cache) &&
                    api.object_get_class != nullptr && api.class_get_method_from_name != nullptr) {
                    Il2CppClass *cache_class = api.object_get_class(inventory_cache);
                    cache_timestamp = cache_class == nullptr
                        ? nullptr
                        : api.class_get_method_from_name(cache_class, "GetLatestTimestamp", 0);
                    cache_update = cache_class == nullptr
                        ? nullptr
                        : api.class_get_method_from_name(cache_class, "UpdateInventory", 0);
                    if (!discard_inventory_cache_timestamp_signature(api, cache_timestamp)) {
                        cache_timestamp = nullptr;
                    }
                    if (!discard_inventory_cache_update_signature(api, cache_update)) {
                        cache_update = nullptr;
                    }
                }
                if (cache_timestamp != nullptr && cache_update != nullptr) {
                    binding->inventory_cache = inventory_cache;
                    binding->inventory_cache_get_latest_timestamp = cache_timestamp;
                    binding->inventory_cache_update_inventory = cache_update;
                    binding->inventory_cache_read_verified = true;
                }
                __android_log_print(
                    binding->inventory_cache_read_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                    kLogTag,
                    "managed diagnostic: inventory cache verified=%d cache=%p timestamp=%p update=%p field=%d",
                    binding->inventory_cache_read_verified ? 1 : 0,
                    inventory_cache,
                    cache_timestamp,
                    cache_update,
                    cache_field_read ? 1 : 0
                );
                // Discard executor binding (RecycleItem + the ItemData data class).
                // ItemData is a nested type with an empty namespace in this build;
                // resolve the actual parameter class from RecycleItem instead of
                // guessing a namespace/name pair.
                const void *recycle_item = api.class_get_method_from_name == nullptr
                    ? nullptr : api.class_get_method_from_name(owner_class, "RecycleItem", 3);
                Il2CppClass *item_data_class = nullptr;
                const bool recycle_contract_verified = discard_recycle_item_signature(
                    api,
                    recycle_item,
                    &item_data_class
                );
                if (recycle_contract_verified) {
                    binding->item_bag_recycle_item = recycle_item;
                    binding->item_data_class = item_data_class;
                    binding->item_data_ctor = api.class_get_method_from_name == nullptr
                        ? nullptr
                        : api.class_get_method_from_name(item_data_class, ".ctor", 10);
                    binding->discard_verified = true;
                    if (api.class_get_field_from_name != nullptr && api.field_get_type != nullptr &&
                        api.type_get_class_or_element_class != nullptr) {
                        const void *medicine_field = api.class_get_field_from_name(
                            item_data_class, "medicineData"
                        );
                        const Il2CppType *medicine_type = medicine_field == nullptr
                            ? nullptr : api.field_get_type(medicine_field);
                        Il2CppClass *medicine_data_class = medicine_type == nullptr
                            ? nullptr : api.type_get_class_or_element_class(medicine_type);
                        if (medicine_data_class != nullptr &&
                            api.class_get_method_from_name != nullptr) {
                            binding->medicine_data_class = medicine_data_class;
                            binding->medicine_data_ctor = api.class_get_method_from_name(
                                medicine_data_class, ".ctor", 0
                            );
                        }
                    }
                }
                __android_log_print(
                    binding->discard_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                    kLogTag,
                    "managed diagnostic: discard binding verified=%d recycle=%p item_data=%p "
                    "medicine_data=%p medicine_ctor=%p",
                    binding->discard_verified ? 1 : 0,
                    recycle_item,
                    item_data_class,
                    binding->medicine_data_class,
                    binding->medicine_data_ctor
                );
            }
        }
        if (strcmp(service.label, "IPlayerService") == 0 && owner != nullptr &&
            api.object_get_class != nullptr && api.class_get_name != nullptr) {
            Il2CppClass *owner_class = api.object_get_class(owner);
            const char *owner_class_name = owner_class == nullptr
                ? nullptr
                : api.class_get_name(owner_class);
            const bool is_player_service = owner_class_name != nullptr &&
                strcmp(owner_class_name, "PlayerService") == 0;
            const void *bag_is_full = is_player_service && api.class_get_method_from_name != nullptr
                ? api.class_get_method_from_name(owner_class, "get_BagIsFull", 0)
                : nullptr;
            const void *max_items = is_player_service && api.class_get_method_from_name != nullptr
                ? api.class_get_method_from_name(owner_class, "get_MaxItems", 0)
                : nullptr;
            if (bag_is_full != nullptr) {
                binding->player_service = owner;
                binding->player_get_bag_is_full = bag_is_full;
                binding->player_get_max_items = max_items;
                binding->inventory_full_read_verified = true;
            }
            __android_log_print(
                binding->inventory_full_read_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "managed diagnostic: inventory full read verified=%d player_service=%p "
                "bag_is_full=%p max_items=%p",
                binding->inventory_full_read_verified ? 1 : 0,
                owner,
                bag_is_full,
                max_items
            );
        }
        if (strcmp(service.label, "IAwardItemBubbleService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(
                api, owner, "Niantic.Holoholo.UI", "AwardItemBubbleService"
            )) {
            binding->award_item_bubble_service_class = service_class;
            bind_spin_item_bubble_service_methods(api, binding, owner);
            update_spin_item_bubble_capabilities(binding);
            __android_log_print(
                binding->spin_item_bubbles_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "managed diagnostic: award item bubble service resolved=%p verified=%d gather=%p spawn=%p",
                owner,
                binding->spin_item_bubbles_verified ? 1 : 0,
                binding->award_bubble_gather_fort_search,
                binding->award_bubble_spawn_bubbles
            );
        }
        if (strcmp(service.label, "IItemInventoryService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(
                api, owner, "Niantic.Holoholo.UI.Items", "ItemInventoryService"
            )) {
            Il2CppClass *owner_class = api.object_get_class(owner);
            const void *get_expiring_items = owner_class == nullptr ||
                api.class_get_method_from_name == nullptr
                ? nullptr
                : api.class_get_method_from_name(owner_class, "get_ExpiringItemsCopy", 0);
            if (get_expiring_items != nullptr) {
                binding->item_inventory_service = owner;
                binding->item_inventory_get_expiring_items = get_expiring_items;
            }
            __android_log_print(
                get_expiring_items != nullptr ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "managed diagnostic: item inventory service verified=%d service=%p expiring=%p",
                get_expiring_items != nullptr ? 1 : 0,
                owner,
                get_expiring_items
            );
        }
        if (strcmp(service.label, "IPokemonBag") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.Internal", "PokemonBagImpl")) {
            // Transfer executor binding (from the 0.427.0 dump): GetPokemon(ulong)
            // resolves the PokemonProto by storage id; ReleasePokemon(PokemonProto)
            // releases it. Execution stays gated behind kTransferExecutionEnabled
            // until the release promise post-condition is verified on device.
            Il2CppClass *owner_class = api.object_get_class(owner);
            const void *get_pokemon = api.class_get_method_from_name == nullptr || owner_class == nullptr
                ? nullptr
                : api.class_get_method_from_name(owner_class, "GetPokemon", 1);
            const void *release = api.class_get_method_from_name == nullptr || owner_class == nullptr
                ? nullptr
                : api.class_get_method_from_name(owner_class, "ReleasePokemon", 1);
            if (get_pokemon != nullptr && release != nullptr) {
                binding->pokemon_inventory = owner;
                binding->pokemon_inventory_get_pokemon = get_pokemon;
                binding->pokemon_inventory_release = release;
                binding->transfer_verified = true;
            }
            __android_log_print(
                binding->transfer_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "managed diagnostic: transfer binding verified=%d bag=%p get_pokemon=%p release=%p",
                binding->transfer_verified ? 1 : 0, owner, get_pokemon, release
            );
        }
        if (strcmp(service.label, "IGameMasterData") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "", "GameMasterData")) {
            binding->game_master_data = owner;
            Il2CppClass *gm_class = api.object_get_class != nullptr
                ? api.object_get_class(owner)
                : nullptr;
            const void *get_item_settings = gm_class == nullptr
                ? nullptr
                : api.class_get_method_from_name(gm_class, "GetItemSettings", 1);
            if (get_item_settings != nullptr) {
                binding->game_master_get_item_settings = get_item_settings;
            }
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: game master data owner=%p get_item_settings=%p",
                owner,
                get_item_settings
            );
        }
        if (strcmp(service.label, "IEncounterPokemon") == 0 && owner != nullptr &&
            api.object_get_class != nullptr && api.class_get_name != nullptr &&
            api.class_get_namespace != nullptr) {
            Il2CppClass *owner_class = api.object_get_class(owner);
            const char *owner_name = owner_class == nullptr
                ? nullptr
                : api.class_get_name(owner_class);
            const char *owner_namespace = owner_class == nullptr
                ? nullptr
                : api.class_get_namespace(owner_class);
            if (owner_name != nullptr && owner_namespace != nullptr &&
                strcmp(owner_name, "EncounterPokemon") == 0 &&
                strcmp(owner_namespace, "Niantic.Holoholo.Encounter") == 0) {
                binding->encounter_pokemon = owner;
            }
        }
        if (strcmp(service.label, "IEncounterState") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.Encounter", "EncounterState")) {
            binding->encounter_state = owner;
        }
        if (strcmp(service.label, "IEncounterInteractionState") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(
                api,
                owner,
                "Niantic.Holoholo.Encounter",
                "EncounterInteractionState"
            )) {
            binding->encounter_interaction = owner;
        }
        if (strcmp(service.label, "IEncounterGuiController") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.Encounter", "EncounterGuiController")) {
            binding->encounter_gui = owner;
        }
        if (strcmp(service.label, "ILocationProvider") == 0 && owner != nullptr &&
            api.object_get_class != nullptr && api.class_get_method_from_name != nullptr) {
            Il2CppClass *owner_class = api.object_get_class(owner);
            const void *get_location = find_runtime_instance_method_in_hierarchy(
                api, owner_class, "get_Location", 0U
            );
            const void *get_has_valid = find_runtime_instance_method_in_hierarchy(
                api, owner_class, "get_HasValidLocation", 0U
            );
            if (get_location != nullptr && get_has_valid != nullptr) {
                binding->location_provider = owner;
                binding->location_get_location = get_location;
                binding->location_get_has_valid = get_has_valid;
                binding->location_read_verified = true;
            }
            __android_log_print(
                binding->location_read_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "managed diagnostic: location provider verified=%d owner=%p location=%p has_valid=%p",
                binding->location_read_verified ? 1 : 0,
                owner, get_location, get_has_valid
            );
        }
        if (strcmp(service.label, "IPokeballService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.Services", "PokeballService")) {
            binding->pokeball_service = owner;
            Il2CppClass *owner_class = api.object_get_class(owner);
            binding->pokeball_service_throw = find_runtime_method_with_two_parameters(
                api,
                owner_class,
                "Throw",
                "UnityEngine.GameObject",
                "UnityEngine.Vector3"
            );
            binding->catch_throw_verified = binding->pokeball_service_throw != nullptr;
            __android_log_print(
                binding->catch_throw_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "managed diagnostic: PokeballService.Throw binding verified=%d owner=%p method=%p",
                binding->catch_throw_verified ? 1 : 0,
                owner,
                binding->pokeball_service_throw
            );
        }
        if (strcmp(service.label, "INearbyPokemonService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.NearbyPokemon", "NearbyPokemonService")) {
            void *map_service = nullptr;
            if (read_runtime_field(api, owner, "ddmr", &map_service)) {
                bind_map_entity_service(api, map_service, binding, "NearbyPokemonService.MapEntityService");
            }
        }
        if (strcmp(service.label, "IMapSceneViewService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.Map", "MapSceneViewService")) {
            binding->map_scene_view_service = owner;
            void *map_content_handler = nullptr;
            if (read_runtime_field(api, owner, "ehkb", &map_content_handler) &&
                runtime_object_is_exact_class(api, map_content_handler, "Niantic.Holoholo.Map", "MapContentHandler")) {
                binding->map_content_handler = map_content_handler;
                void *map_service = nullptr;
                if (read_runtime_field(api, map_content_handler, "egft", &map_service)) {
                    bind_map_entity_service(api, map_service, binding, "MapContentHandler.MapEntityService");
                }
            }
        }
        if (strcmp(service.label, "IDynamicTappablesService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(
                api,
                owner,
                "Niantic.Holoholo.Services.DynamicTappables",
                "DynamicTappablesService"
            )) {
            binding->dynamic_tappables_service = owner;
            binding->dynamic_tappables_tap = find_runtime_method_with_parameter(
                api,
                api.object_get_class(owner),
                "TapTappable",
                "Niantic.Holoholo.Services.DynamicTappables.IDynamicTappable"
            );
            __android_log_print(
                binding->dynamic_tappables_tap == nullptr ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: DynamicTappables.TapTappable binding verified=%d service=%p method=%p",
                binding->dynamic_tappables_tap == nullptr ? 0 : 1,
                owner,
                binding->dynamic_tappables_tap
            );
        }
    }
