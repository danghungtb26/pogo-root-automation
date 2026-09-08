package dev.pogoroot.automation.adapter

enum class GameCapability {
    READ_LIFECYCLE,
    READ_NEARBY,
    /** Runtime can emit a verified screen-tap-to-GeoPoint map observation. */
    READ_MAP_TARGET,
    READ_FORTS,
    READ_INVENTORY,
    READ_POKEMON_STORAGE,
    ENCOUNTER,
    CATCH,
    /** Catch binding confirms CAUGHT and closes the post-catch preview. */
    CATCH_AND_CLOSE_PREVIEW,
    SPIN,
    DISCARD_ITEM,
    TRANSFER_POKEMON,
    MOVE,
    OPEN_ENCOUNTER,
    USE_BERRY,
}
