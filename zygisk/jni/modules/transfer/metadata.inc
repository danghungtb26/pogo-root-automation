// Reads the stored PokemonProto returned by PokemonBagImpl.GetPokemon and
// applies the same keep precedence as the Kotlin TransferPlanner.

struct RuntimePokemonMetadata {
    uint64_t id = 0U;
    int32_t species_id = 0;
    int32_t individual_attack = 0;
    int32_t individual_defense = 0;
    int32_t individual_stamina = 0;
    bool favorite = false;
    bool favorite_known = false;
    bool shiny = false;
    bool shiny_known = false;
    bool special_background = false;
    bool special_background_known = false;
    bool legendary = false;
    bool legendary_known = false;
    bool mythical = false;
    bool mythical_known = false;
    bool iv_known = false;
};

enum class RuntimeTransferDecision : uint32_t {
    kMetadataUnavailable = 0U,
    kKeep = 1U,
    kRelease = 2U,
};

bool invoke_runtime_bool_method(
    const Il2CppApi &api,
    const void *method,
    void *owner,
    void *argument,
    bool *value
) {
    if (method == nullptr || owner == nullptr || argument == nullptr || value == nullptr ||
        api.runtime_invoke == nullptr) return false;
    void *arguments[] = {argument};
    void *exception = nullptr;
    void *boxed = api.runtime_invoke(method, owner, arguments, &exception);
    if (exception != nullptr) return false;
    return read_boxed_bool(api, boxed, value);
}

bool read_runtime_pokemon_metadata(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    void *pokemon,
    RuntimePokemonMetadata *metadata
) {
    if (metadata == nullptr || !runtime_object_is_exact_class(
            api, pokemon, "Holoholo.Rpc", "PokemonProto")) return false;
    *metadata = RuntimePokemonMetadata{};
    const bool core_fields_read =
        read_runtime_field(api, pokemon, "id_", &metadata->id) &&
        read_runtime_field(api, pokemon, "pokemonId_", &metadata->species_id) &&
        read_runtime_field(api, pokemon, "individualAttack_", &metadata->individual_attack) &&
        read_runtime_field(api, pokemon, "individualDefense_", &metadata->individual_defense) &&
        read_runtime_field(api, pokemon, "individualStamina_", &metadata->individual_stamina) &&
        read_runtime_field(api, pokemon, "favorite_", &metadata->favorite);
    metadata->favorite_known = core_fields_read;
    metadata->iv_known = core_fields_read &&
        metadata->individual_attack >= 0 && metadata->individual_attack <= 15 &&
        metadata->individual_defense >= 0 && metadata->individual_defense <= 15 &&
        metadata->individual_stamina >= 0 && metadata->individual_stamina <= 15;
    if (!core_fields_read) return false;

    void *display = nullptr;
    const bool display_read = read_runtime_field(api, pokemon, "pokemonDisplay_", &display) &&
        runtime_object_is_exact_class(api, display, "Holoholo.Rpc", "PokemonDisplayProto");
    if (!display_read) return false;
    metadata->shiny_known = read_runtime_field(api, display, "Shiny", &metadata->shiny);
    void *location_card = nullptr;
    metadata->special_background_known = read_runtime_field(
        api, display, "LocationCard", &location_card
    );
    metadata->special_background = location_card != nullptr;

    if (binding.game_master_data != nullptr &&
        binding.pokemon_proto_is_legendary != nullptr &&
        binding.pokemon_proto_is_mythic != nullptr) {
        metadata->legendary_known = invoke_runtime_bool_method(
            api,
            binding.pokemon_proto_is_legendary,
            pokemon,
            binding.game_master_data,
            &metadata->legendary
        );
        metadata->mythical_known = invoke_runtime_bool_method(
            api,
            binding.pokemon_proto_is_mythic,
            pokemon,
            binding.game_master_data,
            &metadata->mythical
        );
    }
    return true;
}

RuntimeTransferDecision evaluate_runtime_transfer_decision(
    const RuntimeTransferConfig &config,
    const RuntimePokemonMetadata &metadata,
    std::string *reason
) {
    if (reason == nullptr) return RuntimeTransferDecision::kMetadataUnavailable;
    if (config.keep_favorite && !metadata.favorite_known) {
        *reason = "favorite_unknown";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_favorite && metadata.favorite) {
        *reason = "favorite";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_shiny && !metadata.shiny_known) {
        *reason = "shiny_unknown";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_shiny && metadata.shiny) {
        *reason = "shiny";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_hundo && metadata.iv_known &&
        metadata.individual_attack == 15 && metadata.individual_defense == 15 &&
        metadata.individual_stamina == 15) {
        *reason = "hundo";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_special_background && !metadata.special_background_known) {
        *reason = "background_unknown";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_special_background && metadata.special_background) {
        *reason = "special_background";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_legendary && !metadata.legendary_known) {
        *reason = "legendary_unknown";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_legendary && metadata.legendary) {
        *reason = "legendary";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_mythical && !metadata.mythical_known) {
        *reason = "mythical_unknown";
        return RuntimeTransferDecision::kKeep;
    }
    if (config.keep_mythical && metadata.mythical) {
        *reason = "mythical";
        return RuntimeTransferDecision::kKeep;
    }
    if (!metadata.iv_known) {
        *reason = config.keep_unknown_iv ? "iv_unknown" : "iv_unknown_release";
        return config.keep_unknown_iv
            ? RuntimeTransferDecision::kKeep
            : RuntimeTransferDecision::kRelease;
    }
    const double iv_percent = (
        static_cast<double>(metadata.individual_attack) +
        static_cast<double>(metadata.individual_defense) +
        static_cast<double>(metadata.individual_stamina)
    ) * 100.0 / 45.0;
    if (iv_percent < config.minimum_iv_percent_to_keep) {
        char iv_reason[64];
        snprintf(iv_reason, sizeof(iv_reason), "iv_below_threshold_%.2f", iv_percent);
        *reason = iv_reason;
        return RuntimeTransferDecision::kRelease;
    }
    char iv_reason[64];
    snprintf(iv_reason, sizeof(iv_reason), "iv_above_threshold_%.2f", iv_percent);
    *reason = iv_reason;
    return RuntimeTransferDecision::kKeep;
}
