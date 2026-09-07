package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.core.model.Fort
import dev.pogoroot.automation.core.model.FortSnapshot
import dev.pogoroot.automation.core.model.FortType
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.InventorySnapshot
import dev.pogoroot.automation.core.model.ItemStack
import dev.pogoroot.automation.core.model.PokemonIv
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot
import dev.pogoroot.automation.core.model.StoredPokemon

class PogoFortMapper {
    fun map(observation: RawFortObservation): Result<FortSnapshot> = runCatching {
        FortSnapshot(
            observedAtEpochMs = observation.observedAtEpochMs,
            forts = observation.forts.map { raw ->
                require(raw.fortId.isNotBlank()) { "blank fort id" }
                require(raw.latitude in -90.0..90.0) { "invalid fort latitude" }
                require(raw.longitude in -180.0..180.0) { "invalid fort longitude" }

                Fort(
                    fortId = raw.fortId,
                    type = when (raw.type) {
                        RawFortType.POKESTOP -> FortType.POKESTOP
                        RawFortType.GYM -> FortType.GYM
                    },
                    position = GeoPoint(raw.latitude, raw.longitude),
                    spinAvailable = raw.spinAvailable,
                    cooldownEndsAtEpochMs = raw.cooldownEndsAtEpochMs,
                )
            },
        )
    }
}

class PogoInventoryMapper {
    fun map(observation: RawInventoryObservation): Result<InventorySnapshot> = runCatching {
        require(observation.usedSlots >= 0) { "invalid used inventory slots" }
        require(observation.capacity >= 0) { "invalid inventory capacity" }
        require(observation.usedSlots <= observation.capacity) { "inventory usage exceeds capacity" }

        InventorySnapshot(
            observedAtEpochMs = observation.observedAtEpochMs,
            usedSlots = observation.usedSlots,
            capacity = observation.capacity,
            items = observation.items.map { raw ->
                require(raw.itemId > 0) { "invalid item id" }
                require(raw.count >= 0) { "invalid item count" }
                ItemStack(
                    itemId = raw.itemId,
                    itemName = raw.itemName.ifBlank { "#${raw.itemId}" },
                    count = raw.count,
                )
            },
        )
    }
}

class PogoStorageMapper(
    private val speciesNameResolver: SpeciesNameResolver = SpeciesNameResolver { null },
) {
    fun map(observation: RawPokemonStorageObservation): Result<PokemonStorageSnapshot> = runCatching {
        require(observation.usedSlots >= 0) { "invalid used storage slots" }
        require(observation.capacity >= 0) { "invalid storage capacity" }
        require(observation.usedSlots <= observation.capacity) { "storage usage exceeds capacity" }

        PokemonStorageSnapshot(
            observedAtEpochMs = observation.observedAtEpochMs,
            usedSlots = observation.usedSlots,
            capacity = observation.capacity,
            pokemon = observation.pokemon.map(::mapPokemon),
        )
    }

    private fun mapPokemon(raw: RawStoredPokemon): StoredPokemon {
        require(raw.pokemonId.isNotBlank()) { "blank pokemon id" }
        require(raw.speciesId > 0) { "invalid species id" }

        val values = listOf(
            raw.individualAttack,
            raw.individualDefense,
            raw.individualStamina,
        )
        val iv = when {
            values.all { it == null } -> null
            values.any { it == null } -> error("partial IV data for ${raw.pokemonId}")
            else -> PokemonIv(
                attack = raw.individualAttack!!,
                defense = raw.individualDefense!!,
                stamina = raw.individualStamina!!,
            )
        }

        return StoredPokemon(
            pokemonId = raw.pokemonId,
            speciesId = raw.speciesId,
            speciesName = speciesNameResolver.resolve(raw.speciesId) ?: "#${raw.speciesId}",
            iv = iv,
            shiny = raw.shiny,
            specialBackground = raw.specialBackground,
            favorite = raw.favorite,
            legendary = raw.legendary,
            mythical = raw.mythical,
        )
    }
}
