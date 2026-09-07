package dev.pogoroot.automation.pogo

import POGOProtos.Rpc.EncounterOutProto
import POGOProtos.Rpc.GetMapObjectsOutProto

/**
 * Decodes raw Pokemon GO protobuf payloads using the vendored POGOProtos jar.
 *
 * The runtime hook remains responsible for deciding which RPC payload is being
 * delivered. This class only converts a known payload into the stable raw
 * observation models consumed by [PogoGameAdapter].
 */
class PogoProtoDecoder {
    fun decodeEncounter(
        payload: ByteArray,
        observedAtEpochMs: Long,
    ): Result<RawEncounterObservation> = runCatching {
        val response = EncounterOutProto.parseFrom(payload)
        check(response.hasPokemon()) { "encounter response has no Pokemon" }

        val wildPokemon = response.getPokemon()
        check(wildPokemon.hasPokemon()) { "encounter response has no Pokemon data" }
        val pokemon = wildPokemon.getPokemon()

        RawEncounterObservation(
            encounterId = wildPokemon.getEncounterId().toString(),
            speciesId = pokemon.getPokemonIdValue(),
            observedAtEpochMs = observedAtEpochMs,
            individualAttack = pokemon.getIndividualAttack(),
            individualDefense = pokemon.getIndividualDefense(),
            individualStamina = pokemon.getIndividualStamina(),
            shiny = pokemon.getPokemonDisplay().getShiny(),
            latitude = wildPokemon.getLatitude(),
            longitude = wildPokemon.getLongitude(),
        )
    }

    fun decodeMapObjects(
        payload: ByteArray,
        observedAtEpochMs: Long,
        playerLatitude: Double? = null,
        playerLongitude: Double? = null,
    ): Result<RawNearbyObservation> = runCatching {
        val response = GetMapObjectsOutProto.parseFrom(payload)
        val spawns = linkedMapOf<String, RawNearbySpawn>()

        response.getMapCellList().forEach { cell ->
            cell.getWildPokemonList().forEach { wildPokemon ->
                val pokemon = wildPokemon.getPokemon()
                val encounterId = wildPokemon.getEncounterId().toString()
                spawns[encounterId] = RawNearbySpawn(
                    spawnId = encounterId,
                    speciesId = pokemon.getPokemonIdValue(),
                    latitude = wildPokemon.getLatitude(),
                    longitude = wildPokemon.getLongitude(),
                    firstSeenAtEpochMs = observedAtEpochMs,
                    expiresAtEpochMs = null,
                    expiryConfidence = RawExpiryConfidence.UNKNOWN,
                )
            }

            val fortsById = cell.getFortList().associateBy { it.getFortId() }
            cell.getNearbyPokemonList().forEach { nearbyPokemon ->
                val encounterId = nearbyPokemon.getEncounterId().toString()
                if (encounterId in spawns) return@forEach

                val fort = fortsById[nearbyPokemon.getFortId()] ?: return@forEach
                spawns[encounterId] = RawNearbySpawn(
                    spawnId = encounterId,
                    speciesId = nearbyPokemon.getPokedexNumber(),
                    latitude = fort.getLatitude(),
                    longitude = fort.getLongitude(),
                    firstSeenAtEpochMs = observedAtEpochMs,
                    expiresAtEpochMs = null,
                    expiryConfidence = RawExpiryConfidence.UNKNOWN,
                )
            }
        }

        RawNearbyObservation(
            observedAtEpochMs = observedAtEpochMs,
            playerLatitude = playerLatitude,
            playerLongitude = playerLongitude,
            spawns = spawns.values.toList(),
        )
    }
}
