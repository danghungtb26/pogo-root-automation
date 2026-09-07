package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.model.InventorySnapshot
import dev.pogoroot.automation.core.model.ItemStack
import dev.pogoroot.automation.core.model.PokemonIv
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot
import dev.pogoroot.automation.core.model.StoredPokemon

data class MaintenanceSnapshot(
    val protocolVersion: Int,
    val observedAtEpochMs: Long,
    val gameVersionName: String?,
    val gameVersionCode: Long?,
    val discardBindingsReady: Boolean,
    val transferBindingsReady: Boolean,
    val inventory: InventorySnapshot?,
    val storage: PokemonStorageSnapshot?,
    val error: String? = null,
) {
    fun isFresh(nowEpochMs: Long, maxAgeMs: Long = 5_000L): Boolean =
        observedAtEpochMs > 0L && nowEpochMs - observedAtEpochMs in 0L..maxAgeMs
}

object MaintenanceSnapshotParser {
    private const val CURRENT_PROTOCOL = 1

    fun parse(output: String): MaintenanceSnapshot {
        val values = output.lineSequence()
            .map(String::trim)
            .filter { it.isNotEmpty() && '=' in it }
            .associate { line ->
                val separator = line.indexOf('=')
                line.substring(0, separator) to line.substring(separator + 1)
            }

        val protocol = values["maintenance_protocol"]?.toIntOrNull() ?: 0
        val observedAt = values["maintenance_seen_at_epoch_ms"]?.toLongOrNull() ?: 0L
        val itemEntries = values
            .filterKeys { it.startsWith("item_") }
            .mapNotNull { (key, rawCount) ->
                val itemId = key.removePrefix("item_").toIntOrNull() ?: return@mapNotNull null
                val count = rawCount.toIntOrNull() ?: return@mapNotNull null
                if (itemId <= 0 || count < 0) null else ItemStack(itemId, "#$itemId", count)
            }
            .sortedBy(ItemStack::itemId)

        val inventory = values["inventory_capacity"]?.toIntOrNull()?.let { capacity ->
            val used = values["inventory_used_slots"]?.toIntOrNull() ?: itemEntries.sumOf(ItemStack::count)
            if (capacity < 0 || used < 0) null else InventorySnapshot(
                observedAtEpochMs = observedAt,
                usedSlots = used.coerceAtMost(capacity),
                capacity = capacity,
                items = itemEntries,
            )
        }

        val storageEntries = parsePokemon(values["pokemon_storage"].orEmpty())
        val storage = values["pokemon_capacity"]?.toIntOrNull()?.let { capacity ->
            val used = values["pokemon_used_slots"]?.toIntOrNull() ?: storageEntries.size
            if (capacity < 0 || used < 0) null else PokemonStorageSnapshot(
                observedAtEpochMs = observedAt,
                usedSlots = used.coerceAtMost(capacity),
                capacity = capacity,
                pokemon = storageEntries,
            )
        }

        return MaintenanceSnapshot(
            protocolVersion = protocol,
            observedAtEpochMs = observedAt,
            gameVersionName = values["game_version_name"]?.takeIf(String::isNotBlank),
            gameVersionCode = values["game_version_code"]?.toLongOrNull(),
            discardBindingsReady = protocol == CURRENT_PROTOCOL && values["discard_bindings_ready"] == "1",
            transferBindingsReady = protocol == CURRENT_PROTOCOL && values["transfer_bindings_ready"] == "1",
            inventory = inventory,
            storage = storage,
            error = values["maintenance_error"]?.takeIf(String::isNotBlank),
        )
    }

    private fun parsePokemon(raw: String): List<StoredPokemon> {
        if (raw.isBlank()) return emptyList()
        return raw.split(';').mapNotNull { entry ->
            val parts = entry.split('|')
            if (parts.size < 10) return@mapNotNull null
            val pokemonId = parts[0].takeIf(String::isNotBlank) ?: return@mapNotNull null
            val speciesId = parts[1].toIntOrNull() ?: return@mapNotNull null
            val attack = parts[2].toIntOrNull()
            val defense = parts[3].toIntOrNull()
            val stamina = parts[4].toIntOrNull()
            val iv = if (attack != null && defense != null && stamina != null) {
                runCatching { PokemonIv(attack, defense, stamina) }.getOrNull()
            } else {
                null
            }
            StoredPokemon(
                pokemonId = pokemonId,
                speciesId = speciesId,
                speciesName = "#$speciesId",
                iv = iv,
                shiny = parts[5] == "1",
                favorite = parts[6] == "1",
                legendary = parts[7] == "1",
                mythical = parts[8] == "1",
                specialBackground = parts[9] == "1",
                transferMetadataComplete = parts.getOrNull(10) == "1",
            )
        }
    }
}
