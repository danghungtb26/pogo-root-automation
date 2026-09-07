# Feature Analysis — Question Template

Use these questions to think through a feature thoroughly before designing or implementing it. The goal is to uncover hidden complexity, conflicting requirements, and better alternatives before any code is written.

---

## Questions

### 1. What problem does this feature solve?
Describe the user pain or business need. If you can't articulate the problem clearly, the feature is not ready to be designed yet.

### 2. Who benefits from this feature?
- Which users or roles does this affect?
- Is this for all users or a specific segment?
- What does the user currently do without this feature?

### 3. What are the core use cases?
List the primary scenarios this feature must support. Distinguish "must have" from "nice to have."

### 4. What are the edge cases?
Think about:
- Empty states (no data, first-time user)
- Error states (network failure, validation errors, permission denied)
- Boundary inputs (very long text, zero/negative/large numbers, special characters)
- Concurrent actions (user acts twice quickly, data changes while user is on screen)
- Offline behavior

### 5. What are the constraints?
- **Technical:** Platform limitations, performance budget, existing architecture patterns
- **Business:** Timeline, existing contracts, backward compatibility
- **Design:** Consistency with design system, existing UX patterns

### 6. What alternatives were considered?
What other ways could this problem be solved? Why is the proposed approach better than the alternatives?

### 7. How does this interact with existing features?
- Does it share data with another feature?
- Does it affect navigation or app state?
- Could it break or change the behavior of anything that already exists?

### 8. What are the dependencies?
- Does this require a new API endpoint or changes to an existing one?
- Does it depend on another feature being built first?
- Are there third-party dependencies?

### 9. What are the risks?
- What could go wrong during implementation?
- What assumptions are you making that might be wrong?
- What happens if the feature is used in an unexpected way?

### 10. What are the acceptance criteria?
List the specific, verifiable conditions that must be true for this feature to be considered done. Think "given / when / then."
