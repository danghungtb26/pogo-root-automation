---
name: brainstorm
description: "Structured deep-thinking and problem analysis before proposing solutions. Use when: (1) user wants to think through a bug before fixing, (2) planning a new feature and needs to analyze requirements/edge cases/risks/alternatives, (3) considering a refactor and needs to evaluate scope/risk/approach, (4) designing architecture and needs to compare options, (5) diagnosing a performance problem, (6) assessing security concerns, (7) analyzing a UX problem. Trigger on: 'brainstorm', 'think through', 'analyze', 'phân tích', 'suy nghĩ', 'plan', 'deep dive', 'before we implement', 'what could go wrong', 'help me think about', 'let's think through'. Always trigger this skill when the user wants structured thinking before jumping to a solution — even if they don't use the word 'brainstorm'."
---

# Brainstorm

## Purpose

Guide deep, structured thinking before proposing a solution. The goal is to surface the right questions — not just the obvious ones — to uncover hidden assumptions, risks, and alternatives that would be missed by jumping straight to implementation.

Thinking deeply upfront is cheap. Discovering a wrong assumption mid-implementation is expensive.

---

## Output Language (MANDATORY)

**Write every output in Vietnamese by default** — the terminal reply, the saved brainstorm document, all prose, answers, synthesis, and section headings. This is the default whenever the user has not explicitly asked for another language.

- Switch language **only** when the user explicitly requests it (e.g. "viết bằng tiếng Anh", "answer in English", "日本語で").
- **Keep verbatim** and do **not** translate: code, file paths, identifiers, CLI commands, spec IDs (`CR-003`, `R-08`, `VR-002`), and exact spec labels quoted from the source (e.g. Japanese POS labels like `内税品`, `合 計`). Precision beats translation.

---

## Workflow

### Step 1 — Identify Problem Type

Determine the type from context. If unclear, ask which problem type it is: bug / feature / refactor / architecture / performance / security / ux / design-api.

| Type | When to use |
|------|-------------|
| **bug** | Unexpected behavior, crash, wrong output, regression |
| **feature** | New screen, new component, new behavior, new logic |
| **refactor** | Restructure code without changing external behavior |
| **architecture** | System design, data flow, module boundaries, tech decisions |
| **performance** | Slow UI, expensive queries, memory issues, startup time |
| **security** | Vulnerability, data exposure, auth/authz issue |
| **ux** | Confusing flow, accessibility gap, usability problem |
| **design-api** | Design backend/middleware API(s) for a screen or feature — read `remote/NANDEMO000/001`, `docs/design/remote-db-mapping.md`, and app code to derive endpoints with input/output, Oracle query, and flow |

### Step 2 — Load Question Template

Load `references/{type}.md`. This file contains the structured question template for the identified type.

### Step 3 — Answer Every Question

Work through each question in the template:
- Answer based on available context (code, description, design)
- If an answer is unknown, state *what information would resolve it* — don't skip
- State assumptions explicitly where data is lacking
- Flag risks and uncertainties as they appear

Do not skip questions silently. If a question is genuinely irrelevant for this specific case, write one sentence explaining why, then move on.

### Step 3b — Extract Acceptance Criteria from spec (MANDATORY)

**Always** produce an **Acceptance Criteria** section — every brainstorm output must contain it, no exceptions.

1. **Find the spec source** for the screen/feature/bug. Search in this order:
   - `docs/newspec/**` (screen specs — calc rules `CR-xxx`, row rules `R-xx`, validation `VR-xxx`, edge cases `EC-xxx`, layout ASCII)
   - `docs/specs/{id}-{name}/fe.md` (frontend spec)
   - the ticket / requirement text the user provided
2. **Extract the concrete, verifiable rules** that the fix/feature must satisfy — prefer a table with a stable **ID** (e.g. `CR-003`, `R-08`, `VR-002`), the rule name, the exact formula / expected value, and a short acceptance note. Quote the exact spec wording/labels (Japanese POS labels, formulas) — do not paraphrase away the precision.
3. **Tie each reported bug / requirement to its acceptance ID** so the fix is provably "done when X".
4. If **no spec exists**, say so explicitly and write the acceptance criteria you infer from the requirement (mark them `inferred — needs BA confirm`). Never leave the section empty.

This section is what makes "done" objective. It anchors the Synthesis and the verification step.

### Step 4 — Synthesize

After answering all questions, write a synthesis:
- **Key insight** — the most important finding from the analysis
- **Recommended approach** — what to do and why (not how to code it)
- **Risks to watch** — top 2–3 risks to keep in mind during implementation
- **Open questions** — what still needs answering before you can commit to an approach

### Step 5 — Save to File (MANDATORY)

**Always** write the brainstorm document to the issue folder before replying — no exceptions, even if the user did not ask.

**Issue folder convention (SHARED with plan-writer & reviewer):**

```
docs/issues/{YYYY-MM-DD}/{issue-title}/
├── brainstorm.md            ← this skill
├── checklists/**.md         ← plan-writer
└── reviews/{hh-mm-ss}.md    ← reviewer
```

- `{YYYY-MM-DD}` — the date the issue is **first** analyzed. Brainstorm is the step that **creates** this folder, so it stamps the date.
- `{issue-title}` — a short kebab-case slug derived from the problem title (e.g. `customer-display-wrong-mmt`). Keep it **stable and meaningful**: plan-writer and reviewer reuse this exact slug so all three outputs land in one folder.
- **Before creating**, check whether an issue folder for this `{issue-title}` already exists under `docs/issues/*/` (from a prior day/session). If it does, **reuse that folder** (do not create a second dated folder for the same issue).

**Brainstorm file path:** `docs/issues/{YYYY-MM-DD}/{issue-title}/brainstorm.md`

The document structure:

```
# Brainstorm: {problem title}

**Type:** {type}
**Date:** {YYYY-MM-DD}

---

## Analysis

### {Question 1}
{Answer}

### {Question 2}
{Answer}

...

---

## Acceptance Criteria (from spec)

> Source: {docs/newspec/... | docs/specs/{id}/fe.md | ticket} — or "no spec found → inferred".

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| {CR-003 / R-08 / VR-002 / AC-1} | {name} | {formula or expected value} | {ties to which bug/requirement; edge cases} |

- {each reported bug / requirement → its acceptance ID, "done when …"}

---

## Synthesis

### Key Insight
{...}

### Recommended Approach
{...}

### Risks to Watch
- ...

### Open Questions
- ...
```

### Step 6 — Reply with File Link

After saving, reply to the user with:
1. The file path as a clickable reference: `docs/issues/{YYYY-MM-DD}/{issue-title}/brainstorm.md`
2. A concise summary of the **Key Insight** and **Recommended Approach** (3–5 sentences max)
3. Notify the user that the brainstorm file has been saved/updated, pointing to `docs/issues/{YYYY-MM-DD}/{issue-title}/brainstorm.md`.

**Do NOT** dump the full brainstorm content inline in the terminal reply. The file is the source of truth — the terminal reply is only a short summary + file pointer.

### Step 6b — Update existing file (for follow-up questions)

When the user asks follow-up questions or provides new information during an active brainstorm session:
1. Locate the existing brainstorm file for this topic — search `docs/issues/*/{issue-title}/brainstorm.md` and reuse it (do not create a new dated issue folder)
2. Append the new findings as a new numbered section (e.g. `## Section 8 — ...`) at the end of the file
3. Update any **Open Questions** that are now resolved (strike through with `~~question~~` + "→ Resolved in Section N")
4. Save the file
5. Reply with: file path + 2–3 sentence summary of what was added, then stop — do not reprint the whole document

---

## Question Templates — Quick Reference

Full templates with guidance are in `references/{type}.md`.

| Type | Core questions |
|------|----------------|
| **bug** | What happened vs expected? When/where? Root cause? What does the affected code depend on and what depends on it (dependency map)? Scope of impact? Fix risks? How to verify? |
| **feature** | What problem does this solve? Who benefits? What are the core use cases? Edge cases? Alternatives considered? Technical constraints? Risks? |
| **refactor** | Why now? What changes vs what stays? What depends on this code (dependency map)? Scope? Migration strategy? Risks? Expected improvement? |
| **architecture** | Problem statement? Constraints? Quality attributes? Alternative approaches? Trade-offs of each? Integration points? Risks? |
| **performance** | Where is the bottleneck (measured, not guessed)? What are the targets? Root cause? Solutions? Trade-offs? How to measure improvement? |
| **security** | Threat model? What data is at risk? Attack surface? Existing defenses? Vulnerabilities? Mitigations? Compliance requirements? |
| **ux** | Who are the users? What is the user goal? Current pain point? User flows affected? Accessibility? Edge cases for users? Consistency with design system? |
| **design-api** | Which screens/features in scope? What data to read/write? Which Oracle tables (NANDEMO000/001) back it? Endpoint shape per need (/sync, /lookup, /transactions, /ops)? Full contract per endpoint (method, auth, params, request/response JSON, field→column mapping, errors, idempotency, cache)? Actual SQL with bind vars? Middleware pipeline for writes? App-side impact (models/api/repo/DAO/bloc)? Risks & open mappings? |
