package dev.pogoroot.automation.config

import dev.pogoroot.automation.bridge.RuntimeDesiredState

/** Maps durable Android settings to the single native desired-state snapshot. */
object RuntimeDesiredStateMapper {
    fun map(config: HeadlessAutomationConfig): RuntimeDesiredState = RuntimeDesiredState(
        configRevision = config.configRevision,
        enabled = config.enabled,
        catchSpinArmed = config.catchSpinArmed,
        autoCatch = config.autoCatch,
        autoSpin = config.autoSpin,
        autoEncounter = config.autoEncounter,
        catchAll = config.catchAll,
        autoWalkToFort = config.autoWalkToFort,
        spinSettleDelayMs = config.spinSettleDelayMs,
        catchSettleDelayMs = config.catchSettleDelayMs,
        autoDiscard = config.autoDiscard,
        discardLimits = config.discardLimits.toSortedMap(),
        autoTransfer = config.autoTransfer,
        minimumIvPercentToKeep = config.transferMinimumIvPercent,
        keepUnknownIv = true,
        keepShiny = config.transferKeepShiny,
        keepHundo = config.transferKeepHundo,
        keepSpecialBackground = config.transferKeepSpecialBackground,
        keepFavorite = config.transferKeepFavorite,
        keepLegendary = true,
        keepMythical = true,
    )
}

fun HeadlessAutomationConfig.toRuntimeDesiredState(): RuntimeDesiredState =
    RuntimeDesiredStateMapper.map(this)
