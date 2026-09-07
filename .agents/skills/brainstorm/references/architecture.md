# Architecture Analysis — Question Template

Use these questions when making structural design decisions — new module boundaries, data flow choices, tech stack decisions, or integration patterns. Architecture decisions are hard to reverse; slow down and think carefully.

---

## Questions

### 1. What problem are we solving?
State the technical or product problem clearly. Architecture exists to serve a purpose — if the problem is vague, the design will be vague.

### 2. What are the constraints?
- **Hard constraints:** things that cannot change (existing APIs, platform limits, team skill set, timeline)
- **Soft constraints:** preferences that can be overridden with sufficient reason (performance targets, bundle size, consistency with existing patterns)

### 3. What are the quality attributes that matter most?
Rank the attributes that the architecture must optimize for:
- **Maintainability** — easy to change and extend
- **Performance** — fast enough for the use case
- **Testability** — can be tested in isolation
- **Scalability** — can handle growth in data or users
- **Simplicity** — easy to understand and onboard

You cannot maximize all of them at once. Which ones matter most here?

### 4. What are the alternative approaches?
List at least 2–3 alternative designs. If you can only think of one way, you haven't thought hard enough.

### 5. What are the trade-offs of each alternative?
For each option: what does it make easy? What does it make hard? What does it rule out in the future?

### 6. What are the integration points?
- How does this connect to existing modules/systems?
- What data flows in and out?
- Who owns the boundaries — who calls whom?

### 7. How does data flow through the system?
Map the data flow from source to consumer. Ambiguous data ownership causes the most architecture bugs.

### 8. How will this be tested?
- At what level? (unit / integration / e2e)
- Can components be tested in isolation?
- What would a failing test tell you?

### 9. What are the risks?
- What assumptions does this design rely on?
- What happens if those assumptions are wrong?
- What's the worst-case failure mode?

### 10. What is the migration path?
- How do we get from the current state to the new design?
- Can we do it incrementally, or does it require a big-bang change?
- What is the rollback plan if the new design doesn't work?
