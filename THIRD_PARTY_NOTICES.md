# Third-party notices

This project vendors a small set of binary dependencies from the public `Map-A-Droid/PogoEnhancer` source dump for compatibility while the corresponding functionality is being migrated into independent modules.

## Vendored binaries

| File | Upstream | Notes |
| --- | --- | --- |
| `app/libs/POGOProtos-2.60.8.jar` | `Map-A-Droid/PogoEnhancer` | PogoEnhancer identifies POGOProtos as MIT and points to `Furtif/POGOProtos`. Used for Pokémon GO protobuf payload classes. |
| `app/libs/bcpkix-jdk15on-1.60.jar` | `Map-A-Droid/PogoEnhancer` | Bouncy Castle PKIX APIs. Keep only while code paths that need them remain enabled. |
| `app/libs/bcprov-jdk15on-1.60.jar` | `Map-A-Droid/PogoEnhancer` | Bouncy Castle provider APIs. Keep only while code paths that need them remain enabled. |
| `app/libs/parser-1.6.0.aar` | `Map-A-Droid/PogoEnhancer` | Android GPX parser binary bundled by PogoEnhancer. |
| `app/libs/virtualjoystick-1.10.1.aar` | `Map-A-Droid/PogoEnhancer` | `io.github.controlwear.virtual.joystick.android.JoystickView`, used by the built-in joystick overlay. |

The exact upstream Git blob SHA and byte size of each vendored binary are pinned in `scripts/vendor-pogoenhancer-libs.sh`; the vendor script refuses to replace a dependency if either value changes.

PogoEnhancer's own source is distributed under the license in its repository. Bundled third-party components retain their own upstream licenses and notices.
