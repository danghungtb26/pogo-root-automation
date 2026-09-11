package dev.pogoroot.automation.pogo

import java.io.ByteArrayInputStream
import java.io.DataInputStream

/** Decodes the exact inventory payload emitted by the PoGo IItemBag binding. */
object RuntimeInventoryPayloadCodec {
    private const val MAGIC = 0x504F4756 // POGV
    private const val MAX_ITEMS = 4096

    fun decode(
        payload: ByteArray,
        observedAtEpochMs: Long,
    ): Result<RawInventoryObservation> = runCatching {
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == MAGIC) { "invalid runtime inventory payload" }
            val usedSlots = input.readInt()
            val capacity = input.readInt()
            val count = input.readInt()
            require(usedSlots >= 0 && capacity >= 0) { "invalid runtime inventory slots" }
            require(count in 0..MAX_ITEMS) { "invalid runtime inventory count" }
            val items = buildList(count) {
                repeat(count) {
                    val itemId = input.readInt()
                    val itemCount = input.readInt()
                    require(itemId > 0) { "invalid runtime inventory item id" }
                    require(itemCount >= 0) { "invalid runtime inventory item count" }
                    add(
                        RawItemStack(
                            itemId = itemId,
                            itemName = "",
                            count = itemCount,
                        ),
                    )
                }
            }
            require(input.available() == 0) { "trailing runtime inventory payload" }
            RawInventoryObservation(
                observedAtEpochMs = observedAtEpochMs,
                usedSlots = usedSlots,
                capacity = capacity,
                items = items,
            )
        }
    }
}
