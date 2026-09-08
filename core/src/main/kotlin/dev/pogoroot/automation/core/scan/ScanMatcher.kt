package dev.pogoroot.automation.core.scan

import dev.pogoroot.automation.core.model.EncounterSnapshot

enum class ScanMatchType {
    HUNDO,
    SHINY,
}

data class ScanMatch(
    val types: Set<ScanMatchType>,
) {
    val isShundo: Boolean
        get() = ScanMatchType.HUNDO in types && ScanMatchType.SHINY in types
}

class ScanMatcher {
    fun match(encounter: EncounterSnapshot, mode: ScanMode): ScanMatch {
        val types = linkedSetOf<ScanMatchType>()
        if (mode == ScanMode.HUNDO || mode == ScanMode.BOTH) {
            if (encounter.isHundo) types += ScanMatchType.HUNDO
        }
        if (mode == ScanMode.SHINY || mode == ScanMode.BOTH) {
            if (encounter.shiny == true) types += ScanMatchType.SHINY
        }
        return ScanMatch(types)
    }
}
