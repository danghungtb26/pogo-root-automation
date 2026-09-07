package dev.pogoroot.automation.headless

import dev.pogoroot.automation.bridge.MaintenanceSnapshot
import dev.pogoroot.automation.bridge.MaintenanceSnapshotParser
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.root.ProcessRootShell
import dev.pogoroot.automation.root.RootShell
import java.util.concurrent.atomic.AtomicLong

interface MaintenanceRuntime {
    fun readSnapshot(): Result<MaintenanceSnapshot>
    fun execute(action: AutomationAction, config: HeadlessAutomationConfig): Result<Unit>
}

class MaintenanceRuntimeBridge(
    private val rootShell: RootShell = ProcessRootShell(),
) : MaintenanceRuntime {
    private val commandSequence = AtomicLong(System.currentTimeMillis())
    @Volatile
    private var runtimeDir: String? = null

    override fun readSnapshot(): Result<MaintenanceSnapshot> = runCatching {
        val directory = resolveRuntimeDir()
        val result = rootShell.execute("cat $directory/maintenance.snapshot 2>/dev/null", 1_500L)
        check(result.isSuccess) { result.stderr.ifBlank { "maintenance snapshot unavailable" } }
        MaintenanceSnapshotParser.parse(result.stdout)
    }

    override fun execute(action: AutomationAction, config: HeadlessAutomationConfig): Result<Unit> = when (action) {
        is AutomationAction.DiscardItem -> sendCommand(
            action = "discard",
            fields = mapOf(
                "item_id" to action.itemId.toString(),
                "amount" to action.amount.toString(),
            ),
        )

        is AutomationAction.TransferPokemon -> sendCommand(
            action = "transfer",
            fields = mapOf(
                "pokemon_id" to action.pokemonId,
                "protect_hundo" to flag(config.transferKeepHundo),
                "protect_shiny" to flag(config.transferKeepShiny),
                "protect_background" to flag(config.transferKeepSpecialBackground),
                "protect_favorite" to flag(config.transferKeepFavorite),
                "protect_legendary" to "1",
                "protect_mythical" to "1",
                "minimum_iv_percent" to config.transferMinimumIvPercent.toString(),
            ),
        )

        else -> Result.failure(IllegalArgumentException("unsupported maintenance action: $action"))
    }

    private fun sendCommand(action: String, fields: Map<String, String>): Result<Unit> = runCatching {
        val directory = resolveRuntimeDir()
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
        val temp = "$directory/maintenance.command.tmp.$id"
        val commandPath = "$directory/maintenance.command"
        val resultPath = "$directory/maintenance.result"
        val write = rootShell.execute(
            "printf '%s' '$escaped' > $temp && chmod 0600 $temp && mv $temp $commandPath",
            2_000L,
        )
        check(write.isSuccess) { write.stderr.ifBlank { "failed to publish maintenance command" } }

        val deadline = System.currentTimeMillis() + COMMAND_TIMEOUT_MS
        while (System.currentTimeMillis() < deadline) {
            val result = rootShell.execute("cat $resultPath 2>/dev/null", 800L)
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

    private fun resolveRuntimeDir(): String {
        runtimeDir?.let { return it }
        val command = """
            for d in \
              /data/user/0/com.nianticlabs.pokemongo/files/pogo_root_automation \
              /data/user/0/com.nianticlabs.pokemongo.ares/files/pogo_root_automation \
              /data/data/com.nianticlabs.pokemongo/files/pogo_root_automation \
              /data/data/com.nianticlabs.pokemongo.ares/files/pogo_root_automation; do
              if [ -d \"${'$'}d\" ]; then echo \"${'$'}d\"; exit 0; fi
            done
            exit 1
        """.trimIndent().replace("\n", " ")
        val result = rootShell.execute(command, 1_500L)
        check(result.isSuccess) { "Pokémon GO maintenance runtime directory unavailable" }
        return result.stdout.lineSequence().map(String::trim).firstOrNull(String::isNotEmpty)
            ?.also { runtimeDir = it }
            ?: error("Pokémon GO maintenance runtime directory unavailable")
    }

    private fun flag(value: Boolean): String = if (value) "1" else "0"

    companion object {
        private const val COMMAND_TIMEOUT_MS = 2_500L
    }
}
