# UX Analysis — Question Template

Use these questions when analyzing a usability problem, designing a user flow, or reviewing whether a screen serves its users well. Good UX analysis starts from the user's perspective, not the developer's.

---

## Questions

### 1. Who are the users?
- What type of user is this screen or flow for?
- What is their typical context? (in a hurry, first-time, on a small screen, low connectivity)
- What is their level of familiarity with this app and this type of task?

### 2. What is the user's goal?
What is the user trying to accomplish? State it in user language, not system language. ("I want to add a new expense" not "user calls POST /expenses".)

### 3. What is the current pain point?
What is confusing, slow, or frustrating right now?
- Is it a discoverability problem? (user can't find the action)
- Is it a feedback problem? (user doesn't know what happened)
- Is it a cognitive load problem? (too many choices, too much information)
- Is it an error recovery problem? (hard to undo or fix mistakes)

### 4. What are the user flows affected?
Map the steps a user takes from entry point to completion. Look for:
- Steps that require the user to remember something from a previous screen
- Steps where the user must leave and come back
- Dead ends or unclear back navigation
- Places where an error could lose the user's progress

### 5. What are the empty and error states?
For each screen or interaction:
- What does the user see when there is no data yet?
- What does the user see when an action fails?
- Is the error message helpful or cryptic?
- Can the user recover easily?

### 6. What are the edge cases for users?
- What happens with very long text (names, descriptions)?
- What if the user has a slow connection?
- What if the user is interrupted mid-flow?
- What if the user accidentally triggers a destructive action?

### 7. Is this consistent with the rest of the app?
- Does this interaction follow the same patterns as similar flows elsewhere?
- Are the same terms and labels used consistently?
- Does the visual hierarchy match the user's expected priority?

### 8. What are the accessibility considerations?
- Are interactive elements reachable by screen reader?
- Is there sufficient color contrast?
- Are touch targets large enough (minimum 44×44pt)?
- Does the screen work in large text mode?

### 9. What is the ideal redesign?
Propose the simplest change that addresses the root UX problem. Avoid adding complexity — the best UX fix often removes something rather than adds it.

### 10. How will the improvement be validated?
- What would a user test or observation tell you?
- What metric changes would indicate success? (task completion rate, error rate, time-on-task, user feedback)
