package dev.pogoroot.automation.headless

import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.net.URLDecoder
import java.nio.charset.StandardCharsets
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

class AutomationControlServer(
    private val configRepository: AutomationConfigRepository,
    private val engine: HeadlessAutomationEngine,
    private val port: Int = DEFAULT_PORT,
) {
    private val running = AtomicBoolean(false)
    private val acceptExecutor = Executors.newSingleThreadExecutor()
    private val clientExecutor = Executors.newCachedThreadPool()
    @Volatile private var serverSocket: ServerSocket? = null

    fun start() {
        if (!running.compareAndSet(false, true)) return
        acceptExecutor.execute {
            try {
                ServerSocket(port, 16, InetAddress.getByName("127.0.0.1")).use { server ->
                    serverSocket = server
                    while (running.get()) {
                        val socket = runCatching { server.accept() }.getOrNull() ?: break
                        clientExecutor.execute { handle(socket) }
                    }
                }
            } finally {
                serverSocket = null
                running.set(false)
            }
        }
    }

    fun stop() {
        running.set(false)
        runCatching { serverSocket?.close() }
        acceptExecutor.shutdownNow()
        clientExecutor.shutdownNow()
    }

    private fun handle(socket: Socket) {
        socket.use { client ->
            client.soTimeout = 3_000
            val reader = BufferedReader(InputStreamReader(client.getInputStream(), StandardCharsets.UTF_8))
            val requestLine = reader.readLine() ?: return
            val parts = requestLine.split(' ')
            if (parts.size < 2) {
                respond(client, 400, jsonError("invalid request"))
                return
            }

            val method = parts[0].uppercase()
            val target = parts[1]
            while (true) {
                val line = reader.readLine() ?: break
                if (line.isEmpty()) break
            }

            val path = target.substringBefore('?')
            val params = parseQuery(target.substringAfter('?', ""))
            val response = runCatching { route(method, path, params) }.getOrElse { error ->
                ApiResponse(500, jsonError(error.message ?: error::class.java.simpleName))
            }
            respond(client, response.status, response.body)
        }
    }

    private fun route(method: String, path: String, params: Map<String, String>): ApiResponse = when {
        method == "GET" && (path == "/health" || path == "/v1/health") -> ApiResponse(200, "{\"ok\":true}")
        method == "GET" && path == "/v1/status" -> ApiResponse(200, statusJson(engine.snapshot(), configRepository.read()))
        method == "POST" && path == "/v1/start" -> {
            val config = configRepository.update { current -> applyParams(current, params).copy(enabled = true) }
            engine.start()
            ApiResponse(200, statusJson(engine.snapshot(), config))
        }
        method == "POST" && path == "/v1/stop" -> {
            val config = configRepository.update { it.copy(enabled = false) }
            ApiResponse(200, statusJson(engine.snapshot(), config))
        }
        method == "POST" && path == "/v1/config" -> {
            val config = configRepository.update { current -> applyParams(current, params) }
            engine.start()
            ApiResponse(200, configJson(config))
        }
        else -> ApiResponse(404, jsonError("not found"))
    }

    private fun applyParams(config: HeadlessAutomationConfig, params: Map<String, String>): HeadlessAutomationConfig = config.copy(
        autoCatch = params.boolean("autoCatch") ?: params.boolean("catch") ?: config.autoCatch,
        catchThrowQuality = params["throwQuality"]?.let(::parseThrowQuality) ?: config.catchThrowQuality,
        catchCurvePreference = params["curve"]?.let(::parseCurvePreference) ?: config.catchCurvePreference,
        catchEncounterMode = params.boolean("arPlus")?.let { if (it) dev.pogoroot.automation.core.automation.EncounterMode.AR_PLUS else dev.pogoroot.automation.core.automation.EncounterMode.STANDARD }
            ?: config.catchEncounterMode,
        autoSnapshotDuringEncounter = params.boolean("autoSnapshot") ?: config.autoSnapshotDuringEncounter,
        snapshotEncounterMode = params.boolean("snapshotArPlus")?.let { if (it) dev.pogoroot.automation.core.automation.EncounterMode.AR_PLUS else dev.pogoroot.automation.core.automation.EncounterMode.STANDARD }
            ?: config.snapshotEncounterMode,
        mapTapWalkEnabled = params.boolean("mapTapWalk") ?: config.mapTapWalkEnabled,
        autoCloseCatchPreview = params.boolean("autoCloseCatchPreview") ?: config.autoCloseCatchPreview,
        autoSpin = params.boolean("autoSpin") ?: params.boolean("spin") ?: config.autoSpin,
        // The old encounterSweep query parameter is intentionally ignored.
        autoEncounter = params.boolean("autoEncounter") ?: config.autoEncounter,
        autoDiscard = params.boolean("autoDiscard") ?: config.autoDiscard,
        autoTransfer = params.boolean("autoTransfer") ?: config.autoTransfer,
        transferKeepHundo = params.boolean("keepHundo") ?: config.transferKeepHundo,
        transferKeepShiny = params.boolean("keepShiny") ?: config.transferKeepShiny,
        transferKeepSpecialBackground = params.boolean("keepBackground") ?: config.transferKeepSpecialBackground,
        transferKeepFavorite = params.boolean("keepFavorite") ?: config.transferKeepFavorite,
        transferMinimumIvPercent = params["transferMinIv"]?.toDoubleOrNull()?.coerceIn(0.0, 100.0) ?: config.transferMinimumIvPercent,
        structuredAllowedBuildFingerprints = params["buildFingerprints"]
            ?.split(',')
            ?.map(String::trim)
            ?.filter(String::isNotBlank)
            ?.toSet()
            ?: config.structuredAllowedBuildFingerprints,
        berryMode = params["berry"]?.let(::parseBerry) ?: config.berryMode,
        showActionToasts = params.boolean("toasts") ?: config.showActionToasts,
        loopIntervalMs = params["loopIntervalMs"]?.toLongOrNull() ?: config.loopIntervalMs,
    )

    private fun parseBerry(raw: String): BerryMode = runCatching {
        BerryMode.valueOf(raw.trim().uppercase().replace('-', '_').replace(' ', '_'))
    }.getOrDefault(BerryMode.NONE)

    private fun parseThrowQuality(raw: String) = runCatching {
        dev.pogoroot.automation.core.automation.ThrowQualityTarget.valueOf(
            raw.trim().uppercase(),
        )
    }.getOrDefault(dev.pogoroot.automation.core.automation.ThrowQualityTarget.ANY)

    private fun parseCurvePreference(raw: String) = runCatching {
        dev.pogoroot.automation.core.automation.CurvePreference.valueOf(
            raw.trim().uppercase(),
        )
    }.getOrDefault(dev.pogoroot.automation.core.automation.CurvePreference.ANY)

    private fun parseQuery(query: String): Map<String, String> {
        if (query.isBlank()) return emptyMap()
        return query.split('&').mapNotNull { pair ->
            val key = pair.substringBefore('=', "").trim()
            if (key.isEmpty()) return@mapNotNull null
            decode(key) to decode(pair.substringAfter('=', ""))
        }.toMap()
    }

    private fun decode(value: String): String = URLDecoder.decode(value, StandardCharsets.UTF_8.name())

    private fun Map<String, String>.boolean(key: String): Boolean? = when (this[key]?.lowercase()) {
        "1", "true", "yes", "on" -> true
        "0", "false", "no", "off" -> false
        else -> null
    }

    private fun respond(socket: Socket, status: Int, body: String) {
        val statusText = when (status) {
            200 -> "OK"
            400 -> "Bad Request"
            404 -> "Not Found"
            else -> "Internal Server Error"
        }
        val bytes = body.toByteArray(StandardCharsets.UTF_8)
        socket.getOutputStream().buffered().use { output ->
            output.write("HTTP/1.1 $status $statusText\r\n".toByteArray())
            output.write("Content-Type: application/json; charset=utf-8\r\n".toByteArray())
            output.write("Content-Length: ${bytes.size}\r\n".toByteArray())
            output.write("Connection: close\r\n\r\n".toByteArray())
            output.write(bytes)
            output.flush()
        }
    }

    private fun statusJson(status: HeadlessAutomationStatus, config: HeadlessAutomationConfig): String = """
        {"running":${status.running},"enabled":${config.enabled},"mapTapWalk":${config.mapTapWalkEnabled},"autoEncounter":${config.autoEncounter},"autoCatch":${config.autoCatch},"throwQuality":"${config.catchThrowQuality.name}","curve":"${config.catchCurvePreference.name}","arPlus":${config.catchEncounterMode == dev.pogoroot.automation.core.automation.EncounterMode.AR_PLUS},"autoSnapshot":${config.autoSnapshotDuringEncounter},"snapshotArPlus":${config.snapshotEncounterMode == dev.pogoroot.automation.core.automation.EncounterMode.AR_PLUS},"autoCloseCatchPreview":${config.autoCloseCatchPreview},"autoSpin":${config.autoSpin},"autoDiscard":${config.autoDiscard},"autoTransfer":${config.autoTransfer},"berry":"${config.berryMode.name}","toasts":${config.showActionToasts},"runtimeSessionId":${status.runtimeSessionId.jsonStringOrNull()},"runtimeStrongIdentityVerified":${status.runtimeStrongIdentityVerified},"runtimeLifecycle":${status.runtimeLifecycle.jsonStringOrNull()},"runtimeSuspended":${status.runtimeSuspended},"observationSeq":${status.observationSeq ?: "null"},"lastAction":${status.lastAction.jsonStringOrNull()},"lastError":${status.lastError.jsonStringOrNull()},"port":$port}
    """.trimIndent()

    private fun configJson(config: HeadlessAutomationConfig): String = """
        {"enabled":${config.enabled},"mapTapWalk":${config.mapTapWalkEnabled},"autoEncounter":${config.autoEncounter},"autoCatch":${config.autoCatch},"throwQuality":"${config.catchThrowQuality.name}","curve":"${config.catchCurvePreference.name}","arPlus":${config.catchEncounterMode == dev.pogoroot.automation.core.automation.EncounterMode.AR_PLUS},"autoSnapshot":${config.autoSnapshotDuringEncounter},"snapshotArPlus":${config.snapshotEncounterMode == dev.pogoroot.automation.core.automation.EncounterMode.AR_PLUS},"autoCloseCatchPreview":${config.autoCloseCatchPreview},"autoSpin":${config.autoSpin},"autoDiscard":${config.autoDiscard},"autoTransfer":${config.autoTransfer},"keepHundo":${config.transferKeepHundo},"keepShiny":${config.transferKeepShiny},"keepBackground":${config.transferKeepSpecialBackground},"keepFavorite":${config.transferKeepFavorite},"transferMinIv":${config.transferMinimumIvPercent},"berry":"${config.berryMode.name}","buildFingerprints":${config.structuredAllowedBuildFingerprints.toJsonArray()},"toasts":${config.showActionToasts}}
    """.trimIndent()

    private fun String?.jsonStringOrNull(): String = this?.let {
        "\"${it.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n")}\""
    } ?: "null"

    private fun Set<String>.toJsonArray(): String = joinToString(
        prefix = "[",
        postfix = "]",
    ) { it.jsonStringOrNull() }

    private fun jsonError(message: String): String = "{\"ok\":false,\"error\":${message.jsonStringOrNull()}}"

    private data class ApiResponse(val status: Int, val body: String)

    companion object {
        const val DEFAULT_PORT = 8765
    }
}
