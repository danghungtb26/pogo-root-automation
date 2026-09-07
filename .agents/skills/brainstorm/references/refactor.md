# Refactor Analysis — Question Template

Use these questions before refactoring to ensure the change is worthwhile, well-scoped, and safe. A refactor that breaks behavior or balloons in scope is worse than no refactor at all.

---

## Questions

### 1. Why is this refactor needed now?
What specific pain is the current code causing? (hard to test, hard to read, slow to change, causes bugs, inconsistent with the rest of the codebase) Be concrete — "it's messy" is not a sufficient reason.

### 2. What changes and what stays the same?
Clearly separate:
- **What changes:** structure, naming, abstractions, file organization, patterns
- **What stays the same:** external behavior, APIs, data contracts, user-facing output

This distinction is critical. Any change to behavior is a feature or a bug fix, not a refactor.

### 3. What depends on this code? (Dependency map)
Map the relationships **before** defining scope — the scope is a consequence of dependencies, not an assumption:
- **Who calls / imports it?** List every component, screen, hook, or page that uses the refactor target.
- **What does it depend on?** What utilities, hooks, APIs, or data models does it consume?
- **Interface contract:** What props, exports, or function signatures are part of the public API? Changing these forces callers to update too.

If the dependency list is larger than expected, that is a signal to narrow the refactor scope or phase the work.

### 4. What is the scope?
Based on the dependency map above, list every file, component, or module that will be touched. If the list is growing as you think about it, that's a signal to either narrow the scope or split into phases.

### 5. What is driving the decision to do this now?
Is this blocking other work? Part of a larger initiative? Proactive maintenance? Knowing this helps prioritize it correctly and set expectations.

### 6. Are there existing tests covering this code?
- If yes: they are your safety net — run them before and after. Do not delete or skip tests during a refactor.
- If no: should you write tests first before refactoring? (Strongly recommended for complex logic.)

### 7. What is the migration strategy?
- Can this be done in one step, or does it need to be phased?
- Is there a period where old and new coexist? How long?
- Do callers of this code need to be updated? How many? (from Q3 dependency map)

### 8. What are the risks?
- What could break?
- Are there hidden callers (dynamic imports, string-based lookups, config references)?
- Are there runtime behaviors that tests don't cover (timing, device-specific, network-dependent)?

### 9. How will you verify nothing broke?
- Which tests give you confidence?
- Is there a manual QA checklist?
- Are there edge cases that should be tested but currently aren't?

### 10. What is the expected improvement?
After the refactor, what will be concretely better? (e.g., "adding a new payment method will go from touching 5 files to 1", "this module will be independently testable") If you can't articulate a measurable improvement, reconsider whether this refactor is worth the risk.
