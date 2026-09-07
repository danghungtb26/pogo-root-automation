# Performance Analysis — Question Template

Use these questions when diagnosing or planning performance improvements. The cardinal rule of performance work: measure first, optimize second. Never optimize based on intuition alone.

---

## Questions

### 1. What is the observed problem?
Describe the symptom concretely: slow screen load, laggy scroll, high memory usage, long API response time, slow app startup. Include observed numbers if available (e.g., "screen takes ~3 seconds to load").

### 2. What is the performance target?
What does "good enough" look like? Without a target, you don't know when to stop. Common targets:
- Screen load: < 300ms perceived (skeleton shown instantly, data < 1s)
- Scroll: 60fps (no dropped frames)
- API call: < 500ms p95
- App cold start: < 2s

### 3. Have you measured the bottleneck?
Do not optimize based on guesses. Which tool did you use to measure?
- React Native: Flipper, React DevTools Profiler, Performance Monitor
- JS: console.time, performance.now()
- Network: network inspector in dev tools

Where exactly is the time being spent? What is the measured evidence?

### 4. What is the root cause?
Based on measurement, what is causing the slowness?
- **Render thrash:** too many re-renders (missing memoization, unstable references)
- **Heavy computation on the JS thread:** blocking layout/animation
- **Over-fetching:** loading more data than needed
- **Waterfall requests:** sequential API calls that could be parallel
- **Large list without virtualization:** FlatList not configured correctly
- **Image decoding/loading:** large or unoptimized images
- **Memory leak:** objects not being garbage collected

### 5. What are the candidate solutions?
List the approaches that could address the root cause. For each, describe what it changes.

### 6. What are the trade-offs of each solution?
- Complexity added to the codebase?
- Risk of introducing new bugs?
- Maintenance burden?
- Does it solve the root cause or just mask the symptom?

### 7. What is the simplest fix that meets the target?
Avoid over-engineering. A memoization fix is better than a full architecture change if the bottleneck is a single expensive render.

### 8. How will you measure improvement?
Define the before/after measurement criteria. If you can't measure it, you can't know if you fixed it.

### 9. Are there performance regressions to guard against?
Should a performance test or benchmark be added so this doesn't regress in the future?
