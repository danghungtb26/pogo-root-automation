# M2 nearby binding boundary

M2 is split into two independent parts:

1. **Stable/testable nearby core** — raw observation mapping, validation, de-duplication, diffing, expiry confidence, and adapter selection.
2. **Version-specific game binding** — obtain the raw observation from the exact official Pokémon GO build running on the rooted device.

This separation is intentional: reverse-engineered bindings can change on every game update, while the nearby state machine should not.

## Raw observation contract

The version adapter must provide only structured facts it can actually observe:

- stable spawn identifier;
- species id;
- latitude / longitude;
- first-seen timestamp if known;
- expiry timestamp if known;
- expiry confidence: exact / estimated / unknown;
- observation timestamp and optional player position.

`PogoNearbyMapper` rejects malformed coordinates/ids and never upgrades unknown expiry to exact. `NearbySnapshotReducer` handles duplicate ids, expired entries, and add/update/remove diffs.

## Version fail-safe

`GameAdapterRegistry` resolves adapters against the observed package/version. Zero matches is unsupported; multiple matches is ambiguous. Both cases fail closed instead of choosing a binding heuristically.

## What is still missing

No M2 code currently reads Pokémon GO memory or calls game functions. Implementing the real raw-observation producer requires analysis against the exact official client build that will be tested. Binary/build-specific artifacts must not be committed to this repository; keep derived signatures/mappings small and version-scoped.
