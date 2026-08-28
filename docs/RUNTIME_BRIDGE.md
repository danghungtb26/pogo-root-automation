# M1 runtime bridge

M1 intentionally proves rooted process lifecycle before any game-state hooking.

## Flow

```text
Pokémon GO starts
  -> Zygisk preAppSpecialize sees target process
  -> module sends a small versioned RuntimeEvent to Zygisk companion
  -> companion (root) atomically writes /data/adb/pogo_root_automation/runtime.status
  -> controller executes runtime-status.sh through su
  -> script validates that the recorded PID still belongs to the same process
  -> script also reports installed package version
  -> controller parses key=value output into RuntimeSnapshot
```

This is deliberately a polling bridge for M1. It has two useful properties:

1. the injected library does not need to stay resident merely to report lifecycle;
2. a stale status file cannot report `connected`, because the root status script verifies `/proc/<pid>/cmdline` and process liveness.

M2 may replace or augment this transport when high-frequency nearby snapshots are required.

## Root state

The companion writes only:

- protocol version;
- PID;
- target process name;
- observation timestamp.

The controller-side root script derives current liveness and installed game version at read time.

## Current limitation

There is no explicit process-death callback from this M1 stub. A dead/reused PID is detected by checking both liveness and `/proc/<pid>/cmdline`. Runtime state therefore converges to disconnected on the controller's next poll.
