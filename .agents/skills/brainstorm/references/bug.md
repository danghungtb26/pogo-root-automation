# Bug Analysis — Question Template

Use these questions to fully understand a bug before proposing a fix. Skipping questions often leads to fixing the symptom, not the cause.

---

## Questions

### 1. What is the current behavior?
Describe exactly what happens — error message, wrong value, crash, missing output. Be specific, not vague ("it doesn't work" is not enough).

### 2. What is the expected behavior?
What should happen instead? Reference the spec, design, or user expectation.

### 3. When and where does it occur?
- What conditions trigger it? (specific input, screen state, user action, device, OS version)
- Is it consistent or intermittent?
- Does it happen in all environments (dev/staging/prod) or only some?

### 4. Can you reproduce it reliably?
Describe the steps to reproduce. If not reliably reproducible, what makes it appear/disappear?

### 5. What is the root cause?
Trace the problem to its origin — not the surface symptom. Look at:
- Data flow: where does wrong data enter the system?
- Logic: which condition or branch is incorrect?
- State: what state is unexpectedly set or missing?
- Timing: is there a race condition or async issue?

### 6. What is this code connected to? (Dependency map)
Map the relationships **before** reading deep into code — this determines what to read and what the fix blast radius is:
- **Upstream dependencies**: what data sources, APIs, hooks, utilities, or props feed into the buggy code?
- **Downstream consumers**: what components, screens, or flows consume or depend on the output of the buggy code?
- **Shared code**: is the affected code used in multiple places? If so, where?

This map tells you: (a) what to read in Step 3, and (b) what could break if you change it.

### 7. What is the scope of impact?
- Which users/flows are affected?
- Are there other places in the code where the same bug pattern could exist (from the dependency map above)?

### 8. Why did this bug occur? (prevention mindset)
What allowed this bug to exist — missing validation, wrong assumption, unclear spec, no test coverage? Understanding this helps prevent similar bugs.

### 9. What are the risks of the fix?
- What could break if we change this?
- Are there other callers of the affected code (from Q6 dependency map)?
- Does the fix require a data migration or backward-compatibility consideration?

### 10. What is the minimal correct fix?
Avoid over-engineering the fix. What is the smallest change that correctly resolves the root cause without introducing new risk?

### 11. How will we verify the fix?
- What test case would catch this bug?
- What manual steps would confirm the fix?
- Should a regression test be added?
