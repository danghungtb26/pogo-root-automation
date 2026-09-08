package dev.pogoroot.automation.scan

import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.scan.ScanMatchType
import dev.pogoroot.automation.core.scan.ScanMatcher
import dev.pogoroot.automation.core.scan.ScanMode
import dev.pogoroot.automation.core.scan.ScanResultSummary

/**
 * Small process-local result store shared by the headless service and overlay.
 *
 * Scan results belong to the current runtime session, so they intentionally do
 * not survive an app process restart. The shared state in the companion object
 * is required because the service and UI each create their own repository
 * instance while still running in the same app process.
 */
class ScanResultRepository {
    companion object {
        private const val MAX_RESULTS = 100

        private val lock = Any()
        private val hundoResults = ArrayList<ScanResultSummary>(MAX_RESULTS)
        private val shinyResults = ArrayList<ScanResultSummary>(MAX_RESULTS)
    }

    private val matcher = ScanMatcher()

    fun read(matchType: ScanMatchType): List<ScanResultSummary> {
        synchronized(lock) {
            return listFor(matchType).toList()
        }
    }

    fun recordEncounter(encounter: EncounterSnapshot?) {
        if (encounter == null) return
        matcher.match(encounter, ScanMode.BOTH).types.forEach { matchType ->
            record(
                ScanResultSummary.fromEncounter(
                    encounter,
                    matchType,
                    encounter.encounterId,
                ),
            )
        }
    }

    fun record(summary: ScanResultSummary?) {
        if (summary == null) return
        synchronized(lock) {
            val results = listFor(summary.matchType)
            val identity = identityOf(summary)
            results.removeAll { identityOf(it) == identity }
            results.add(0, summary)
            while (results.size > MAX_RESULTS) {
                results.removeAt(results.lastIndex)
            }
        }
    }

    fun clear() {
        synchronized(lock) {
            hundoResults.clear()
            shinyResults.clear()
        }
    }


    private fun listFor(matchType: ScanMatchType): ArrayList<ScanResultSummary> =
        if (matchType == ScanMatchType.HUNDO) hundoResults else shinyResults

    private fun identityOf(summary: ScanResultSummary): String =
        "${summary.spawnId}|${summary.encounterId}"
}
