package dev.pogoroot.automation.adapter

enum class GameCapability {
    READ_LIFECYCLE,
    READ_NEARBY,
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
