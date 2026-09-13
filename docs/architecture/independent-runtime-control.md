# Native runtime ownership

Native owns feature activation, scheduling, target selection, action guards,
execution, Promise completion and pause/resume state. Kotlin owns Android UI,
service lifetime, durable user preferences and authenticated config transport.
There is one automation execution path; there is no split runtime mode.

## Responsibilities

| Concern | Owner |
| --- | --- |
| Settings, saved master/arm intent, toast rendering | Kotlin UI/service |
| Connect/reconnect, session validation, CONFIG_SET acknowledgement | Kotlin transport |
| Host START/STOP requested by service lifetime or user | Kotlin sends intent; native executes |
| Managed readiness delay, retries, map readiness | Native host |
| Per-module enable/disable and availability | Native registry/config |
| Catch/spin scans, selection, timing and outcome tracking | Native catch_spin |
| Post-catch keep policy, release and completion | Native transfer |
| Inventory limits, discard scheduling and completion | Native discard |
| Auto-walk fort choice, pause/resume, arrival and retargeting | Native catch_spin |
| Execute a received destination with Android mock location | Kotlin location service |
| Reject stale instructions, stop location on disconnect/expired lease | Kotlin service transport safety |

Kotlin no longer contains module descriptors, activation predicates, execution
locks or a gameplay event dispatcher. `RuntimeBridgeClient` has no per-module
ENABLE/DISABLE sender. Old module-control requests are explicitly rejected by
native; callers must change config or stop the host. Wire IDs remain reserved
for compatibility and module registration telemetry.

## Start and reconnect

1. The foreground service restores saved intent and connects to the injected
   runtime. Opening/closing the controller Activity has no effect on module state.
2. START prepares the native host without immediately calling managed discovery.
3. Native waits three seconds, then probes; failed readiness retries every five
   seconds. Discovery is never repeated while a module observes managed state.
4. Native advertises verified runtime identity/capabilities. Kotlin sends all
   three revisioned config snapshots and waits for their acknowledgements.
5. Native waits for matching revisions and a verified map, evaluates each
   module's desired/available state, and starts transfer/discard before catch-spin.
6. Native executes independently between config updates. Kotlin drains telemetry
   for UI and service instructions; it does not turn snapshots into actions.

DIAGNOSTIC remains an explicit troubleshooting request exposed through the UI/API.
There is no automatic diagnostic timer in Kotlin.

## Auto-walk service instructions

`RuntimeAutoFortNavigation` evaluates the same native map snapshot used by
catch-spin. Missing/incomplete map data or player position produces STOP.
Nearby Pokémon pause walking; native action/promise guards pause it before
catch/spin and while other work blocks catch-spin. Complete empty scans select
an available PokéStop, revalidate the current fort, and report arrival once.

Navigation uses observation type 11, payload version 1 (`POGW`): kind
(STOP/WALK/ARRIVED), fort ID, target latitude/longitude, and stop reason.
It is separate from map-tap observations; `READ_MAP_TARGET` guards remain intact.

WALK is renewed on each native scan. Kotlin validates the session, payload and
monotonic timestamp and hands the exact destination to its location service.
Instructions expire after five seconds without renewal, including while waiting
for the location service to start. Repeated renewals do not repeat toasts or
restart the walk. ARRIVED only produces UI feedback; native selects the next fort.

## Stop and recovery

Native joins the observer before resetting module state and releasing Promise
handles. Host STOP disables all modules and clears configs; game process death
destroys all native workers. Kotlin cancels location on a lost connection and
resends configs only after the next session passes identity/readiness guards.
An explicit saved OFF choice stays OFF across process restarts.

The removed `structured-only-source-guard.sh` checked symbols from the retired
mode split. CI instead compiles/tests the app and native readiness/navigation/
telemetry contracts. Removing that script does not remove runtime identity,
freshness, capability or fail-closed checks.

See [verification and device limits](../automation-lifecycle-verification.md).
