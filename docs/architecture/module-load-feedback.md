# Runtime module load feedback

Native runtime feature modules are registered once per target-process runtime session. Registration status is published back through the existing runtime result transport so the Android service can surface immediate user feedback without mixing bootstrap state into gameplay events.

```text
binding_probe_thread
  -> initialize_runtime_modules(fd)
     -> register CATCH_SPIN
     -> register DISCARD
     -> register TRANSFER
     -> register ENCOUNTER
     -> send runtime-module-load:<wireId> result for each module
        -> root companion/broker
        -> RuntimeBridgeClient
        -> RuntimeModuleLoadStatus callback
        -> HeadlessAutomationService
        -> MODULE_LOADED / MODULE_LOAD_FAILED toast
```

The synthetic bootstrap result IDs are intercepted inside `RuntimeBridgeClient` and are not exposed to the structured gameplay event queue. Notifications are deduplicated per `runtimeSessionId + module`.

Module-load toasts are diagnostic bootstrap feedback and are intentionally shown even when normal action toasts are disabled. A successful load means the native module registered with the runtime host; it does not imply that every version-scoped binding required to enable that module is available. Binding availability is still evaluated when the service enables the module.
