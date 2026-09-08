package dev.pogoroot.automation.overlay

import dev.pogoroot.automation.core.time.TeleportCooldownMode
import dev.pogoroot.automation.headless.AutomationConfigRepository
import dev.pogoroot.automation.headless.BerryMode

internal data class AutomationSettingsCategory(
    val id: String,
    val title: String,
    val summary: String,
)

internal object AutomationSettingsCatalog {
    fun categories(
        repository: AutomationConfigRepository,
        cooldownMode: TeleportCooldownMode,
    ): List<AutomationSettingsCategory> {
        val config = repository.read()
        return listOf(
            AutomationSettingsCategory(
                id = "automation",
                title = "Automation",
                summary = "Master ${onOff(config.enabled)} · Catch ${onOff(config.autoCatch)} · Preview close ${onOff(config.autoCloseCatchPreview)}",
            ),
            AutomationSettingsCategory(
                id = "discard",
                title = "Item discard",
                summary = "Auto discard ${onOff(config.autoDiscard)}",
            ),
            AutomationSettingsCategory(
                id = "transfer",
                title = "Pokémon transfer",
                summary = "Auto transfer ${onOff(config.autoTransfer)} · IV ≥ ${config.transferMinimumIvPercent.toInt()}%",
            ),
            AutomationSettingsCategory(
                id = "berry",
                title = "Encounter berry",
                summary = berryLabel(config.berryMode),
            ),
            AutomationSettingsCategory(
                id = "feedback",
                title = "Feedback",
                summary = "Action toasts ${onOff(config.showActionToasts)}",
            ),
            AutomationSettingsCategory(
                id = "cooldown",
                title = "Cooldown display",
                summary = cooldownModeLabel(cooldownMode),
            ),
        )
    }

    fun titleFor(id: String): String = when (id) {
        "automation" -> "Automation"
        "discard" -> "Item discard"
        "transfer" -> "Pokémon transfer"
        "berry" -> "Encounter berry"
        "feedback" -> "Feedback"
        "cooldown" -> "Cooldown display"
        else -> "Automation Settings"
    }

    fun berryLabel(mode: BerryMode): String = when (mode) {
        BerryMode.NONE -> "Off"
        BerryMode.RAZZ -> "Razz Berry"
        BerryMode.NANAB -> "Nanab Berry"
        BerryMode.PINAP -> "Pinap Berry"
        BerryMode.GOLDEN_RAZZ -> "Golden Razz Berry"
        BerryMode.SILVER_PINAP -> "Silver Pinap Berry"
    }

    private fun cooldownModeLabel(mode: TeleportCooldownMode): String = when (mode) {
        TeleportCooldownMode.CURRENT_POSITION -> "Current position"
        TeleportCooldownMode.LAST_ACTIVE -> "Last active"
    }

    private fun onOff(value: Boolean): String = if (value) "ON" else "OFF"
}
