package dev.pogoroot.automation.headless

import dev.pogoroot.automation.bridge.MaintenanceSnapshot
import dev.pogoroot.automation.bridge.MaintenanceSnapshotParser
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.root.ProcessRootShell
import dev.pogoroot.automation.root.RootShell
import java.util.concurrent.atomic.AtomicLong

class MaintenanceRuntimeBridge(
    private val rootShell: RootShell = ProcessRootShell(),
) {
    private val commandSequence = AtomicLong(System.currentTimeMillis())

    fun readSnapshot(): Result<MaintenanceSnapshot> = runCatching {
        val result = rootShell.execute("cat $SNAPSHOT_PATH 2>/dev/null", 1_500L)
        check(result.isSuccess) { result.stderr.ifBlank { "maintenance snapshot unavailable" } }
        MaintenanceSnapshotParser.parse(result.stdout)
    }

    fun execute(action: AutomationAction): Result<Unit> = when (action) {
        is AutomationAction.DiscardItem -> sendCommand(
            action = "discard",
            fields = mapOf(
                "item_id" to action.itemId.toString(),
                "amount" to action.amount.toString(),
            ),
        )

        is AutomationAction.TransferPokemon -> sendCommand(
            action = "transfer",
            fields = mapOf("pokemon_id" to action.pokemonId),
        )

        else -> Result.failure(IllegalArgumentException("unsupported maintenance action: $action"))
    }

    private fun sendCommand(action: String, fields: Map<String, String>): Result<Unit> = runCatching {
        val id = commandSequence.incrementAndGet()
        val payload = buildString {
            append("maintenance_command_protocol=1\n")
            append("command_id=$id\n")
            append("action=$action\n")
            fields.forEach { (key, value) ->
                require(value.none { it == '\n' || it == '\r' }) { "invalid command value" }
                append(key).append('=').append(value).append('\n')
            }
        }
        val escaped = payload.replace("'", "'\\''")
        val temp = "$COMMAND_PATH.tmp.$id"
        val write = rootShell.execute(
            "mkdir -p $STATE_DIR && printf '%s' '$escaped' > $temp && chmod 0600 $temp && mv $temp $COMMAND_PATH",
            2_000L,
        )
        check(write.isSuccess) { write.stderr.ifBlank { "failed to publish maintenance command" } }

        val deadline = System.currentTimeMillis() + COMMAND_TIMEOUT_MS
        while (System.currentTimeMillis() < deadline) {
            val result = rootShell.execute("cat $RESULT_PATH 2>/dev/null", 800L)
            if (result.isSuccess) {
                val values = result.stdout.lineSequence()
                    .map(String::trim)
                    .filter { it.isNotEmpty() && '=' in it }
                    .associate { line -> line.substringBefore('=') to line.substringAfter('=') }
                if (values["command_id"]?.toLongOrNull() == id) {
                    check(values["success"] == "1") {
                        values["message"].orEmpty().ifBlank { "maintenance command failed" }
                    }
                    return@runCatching
                }
            }
            Thread.sleep(80L)
        }
        error("maintenance command timed out")
    }

    companion object {
        private const val STATE_DIR = "/data/adb/pogo_root_automation"
        private const val SNAPSHOT_PATH = "$STATE_DIR/maintenance.snapshot"
        private const val COMMAND_PATH = "$STATE_DIR/maintenance.command"
        private const val RESULT_PATH = "$STATE_DIR/maintenance.result"
        private const val COMMAND_TIMEOUT_MS = 2_500L
    }
}
