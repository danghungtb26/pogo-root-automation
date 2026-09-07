package dev.pogoroot.automation.bridge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MaintenanceSnapshotParserTest {
    @Test
    fun `parses inventory and protected pokemon metadata`() {
        val now = System.currentTimeMillis()
        val snapshot = MaintenanceSnapshotParser.parse(
            """
            maintenance_protocol=1
            maintenance_seen_at_epoch_ms=$now
            game_version_name=0.999.0
            game_version_code=999
            discard_bindings_ready=1
            transfer_bindings_ready=1
            inventory_capacity=400
            inventory_used_slots=350
            item_1=250
            item_2=100
            pokemon_capacity=500
            pokemon_used_slots=2
            pokemon_storage=a|25|15|15|15|1|0|0|0|1|1;b|4|3|4|5|0|0|0|0|0|1
            """.trimIndent(),
        )

        assertTrue(snapshot.discardBindingsReady)
        assertTrue(snapshot.transferBindingsReady)
        assertTrue(snapshot.isFresh(now + 1_000L))
        assertEquals(250, snapshot.inventory!!.items.first { it.itemId == 1 }.count)
        assertEquals(2, snapshot.storage!!.pokemon.size)
        val protected = snapshot.storage.pokemon.first { it.pokemonId == "a" }
        assertTrue(protected.iv!!.isHundo)
        assertTrue(protected.shiny)
        assertTrue(protected.specialBackground)
        assertTrue(protected.transferMetadataComplete)
    }

    @Test
    fun `missing completeness flag keeps pokemon fail closed`() {
        val snapshot = MaintenanceSnapshotParser.parse(
            """
            maintenance_protocol=1
            maintenance_seen_at_epoch_ms=1
            pokemon_capacity=1
            pokemon_used_slots=1
            pokemon_storage=a|25|1|1|1|0|0|0|0|0
            """.trimIndent(),
        )

        assertFalse(snapshot.storage!!.pokemon.single().transferMetadataComplete)
    }

    @Test
    fun `wrong protocol never enables destructive bindings`() {
        val snapshot = MaintenanceSnapshotParser.parse(
            """
            maintenance_protocol=99
            maintenance_seen_at_epoch_ms=1
            discard_bindings_ready=1
            transfer_bindings_ready=1
            """.trimIndent(),
        )

        assertFalse(snapshot.discardBindingsReady)
        assertFalse(snapshot.transferBindingsReady)
        assertNotNull(snapshot)
    }
}
