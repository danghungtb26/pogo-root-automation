# API Design Analysis — Question Template

Use these questions when designing API(s) that back a screen / feature and are served by a middleware sitting in front of the **remote Oracle DB** (see `docs/design/remote-db-mapping.md`, `remote/NANDEMO000`, `remote/NANDEMO001`). The output of this brainstorm should be a concrete endpoint contract — URL, verb, request/response JSON, Oracle query, field mapping — not a vague sketch.

> **Reference example:** `docs/brainstorm/2026-04-14-pos-api-design-remote-db.md` — follow its structure and level of detail (endpoint table → per-endpoint SQL → per-endpoint request/response → mapping table → errors → idempotency/caching notes).

---

## Mandatory pre-reading

Before answering, **actually read** (don't guess):

1. **Remote DB source-of-truth:**
   - `remote/NANDEMO000/` and `remote/NANDEMO001/` — DDL / sample data / column semantics.
   - `docs/design/remote-db-mapping.md` — canonical table → app-domain mapping.
   - `docs/design/remote-db-sync-implementation-plan.md` — sync conventions already decided.
   - `docs/design/remote-db-login-and-timechange.md` — auth + business-date conventions.
2. **App code for the screen / feature:**
   - Feature module under `lib/features/{feature}/` — identify BLoC events, state fields, repository calls.
   - Repository + data-source under `lib/shared/repositories/`, `lib/shared/api/` — see what the app already expects.
   - Models under `lib/shared/models/` — field names/types the API must satisfy.
3. **Prior API brainstorms** in `docs/brainstorm/*-api-design-*.md` — keep conventions consistent (naming, auth, paging, error codes).

If any of these files don't exist or are empty, state that explicitly in the analysis — do not invent schema.

---

## Questions

### 1. What screens / features are in scope?
List every screen or feature the API set must serve. For each: note the Figma / spec reference, the main user actions, and the data it displays or commits.

### 2. For each screen, what data does it need and what actions does it trigger?
Split into:
- **Read / display** — what rows, with what filters, at what freshness (cached vs realtime)?
- **Write / commit** — what operations (create, update, void, issue), what atomicity requirement?
- **Lookup** — on-demand queries the user triggers (scan JAN, enter code, search member).

### 3. Which Oracle tables back each piece of data?
Map every data item to its Oracle source. Use `remote-db-mapping.md` as the starting point. For joins, name every table and the join keys. If a mapping is missing, flag it as an **Open Question** — don't guess column names.

### 4. What is the right endpoint shape for each need?
Classify each endpoint into one of:
- **`/sync/*`** — bulk download of master data, paged + delta (`since`, `cursor`, `limit`), idempotent.
- **`/lookup/*`** — single-row on-demand, online only, not cached.
- **`/transactions/*`, `/settlements/*`, etc.** — write + read-back for business facts.
- **`/ops/*`** — operational state (business date, lock status, permission refresh).

Justify the choice per endpoint. Prefer slim composable endpoints (one endpoint ≈ one table or one tight join) over fat aggregates — the middleware does the mapping, not the app.

### 5. For each endpoint, specify the full contract
Produce, per endpoint:

- **Method + path** (e.g. `GET /sync/products`)
- **Auth** (bearer JWT? which claims are read — `storeCode`, `areaCode`, `clerkCode`?)
- **Query / path params** — name, type, required, default, validation
- **Request body** (for POST/PUT) as concrete JSON, with realistic example values
- **Response body** as concrete JSON, camelCase, with realistic example values
- **Mapping table** — `App field → Remote source (table.column)` — every response field must be traceable
- **Errors** — machine-readable codes (`PRODUCT_NOT_FOUND`, `EIGYO_DATE_MISMATCH`, …) + when they fire
- **Idempotency** — does this need a client-generated key (`clientTxId`)? What is the merge rule on retry?
- **Caching** — app-side TTL, server-side cache, or no cache? Why?

### 6. For each endpoint, write the Oracle query
Provide the actual SQL the middleware will run — not pseudo-SQL. Include:
- `SELECT` with real column names from NANDEMO000/001 (UPPER_SNAKE_CASE).
- Every `JOIN` with join key + any filter (`DAIHYO_JAN_FLAG='1'`, `CS_BT_KUBUN='1'`, `POS_DATA_SAKUSEI_FLAG='1'`, etc.).
- Bind variables prefixed with `:` (`:storeId`, `:since`, `:limit`, `:areaCode`).
- For `/sync/*`: `WHERE KOSIN_DATE > :since ORDER BY KOSIN_DATE, <pk> FETCH FIRST :limit ROWS ONLY`.
- For writes: list **every** `INSERT` / `UPDATE` in order, noting which are inside the same Oracle transaction.

If a column referenced by the mapping doesn't exist in DDL, stop and flag — do not fabricate.

### 7. What is the middleware pipeline for each write endpoint?
For `POST` endpoints that touch multiple tables, spell out the numbered pipeline (like §1.7 of the reference doc):
1. Idempotency check (which table, which key).
2. Validation (JWT claim vs payload, business date, store lock).
3. Ordered `INSERT` / `UPDATE` list with table names and PK composition.
4. Side effects (point journal, member balance update, settlement lock).
5. Commit boundary.

### 8. What is the data flow between app, middleware, Oracle?
For each endpoint, describe:
- Trigger in the app (event, screen lifecycle, user action).
- What the app sends.
- What the middleware does (map, validate, query, transform).
- Oracle query / transaction.
- What comes back and how the app consumes it (write to Drift cache? update BLoC state? display directly?).

### 9. What conventions are shared across all endpoints?
Fix these once so every endpoint follows them:
- Paging (`since` + `cursor` + `limit`, `nextCursor=null` terminator, `fullReload` flag).
- Casing (camelCase JSON ↔ UPPER_SNAKE_CASE Oracle, done in middleware).
- Timestamps (ISO-8601 `+09:00`, `eigyoDate` as `YYYY-MM-DD`).
- Error envelope shape.
- Auth header (`Authorization: Bearer <jwt>`), claim names.
- Rate / size limits.

### 10. What are the risks and open questions?
- Columns whose semantics are unclear from DDL alone.
- Tables without a `KOSIN_DATE` (how will delta work?).
- PK ambiguity on write tables (same JAN twice in one receipt — cumulate or use `SUB_NO`?).
- Area / region scoping (`:areaCode` source — JWT claim vs lookup).
- Race conditions (settlement lock, point balance).
- Anything the mapping doc doesn't cover.

### 11. What does the app side need to change?
List the minimal app-side changes implied by this API set:
- New models / freezed classes under `lib/shared/models/{domain}/`.
- New Retrofit client methods under `lib/shared/api/{domain}/`.
- Repository methods (remote + local fallback) under `lib/shared/repositories/{domain}/`.
- Drift DAO / schema additions (for `/sync/*` consumers).
- BLoC events / states touched on each screen.

Keep this list tight — the brainstorm is about the **API**, not the full implementation.

---

## Output format (save to `docs/brainstorm/YYYY-MM-DD-{topic}.md`)

Follow the structure of the reference example:

```
# {topic} — backed by Remote DB (NANDEMO000/001)

**Type:** design-api · **Date:** {today}

{1–2 paragraph context: which screens, what convention (slim composable), which prior brainstorms it extends}

---

## 1. Endpoint Contracts

{Markdown table grouping endpoints by /sync, /lookup, /transactions, /ops —
columns: #, Endpoint, Screen, Auth, Purpose}

---

### 1.1 Convention chung cho /sync/*
{paging, response template, error codes — once}

### 1.2 GET /sync/{first}
**Query Oracle:**
```sql
SELECT ...
```
**Response item:**
```json
{ ... }
```
**Mapping:**
| App field | Remote source |
|---|---|
| ... | ... |

{repeat 1.3, 1.4, ... for every endpoint}

### 1.N POST /transactions  (or any write endpoint)
**Request:** ```json ...```
**Middleware pipeline (Oracle transaction):**
1. ...
**Response:** ```json ...```
**Errors:** `...`
**Idempotency key:** ...

---

## 2. Cross-cutting conventions
{auth, paging, casing, timestamps, errors — summarized}

---

## 3. App-side impact
{bullet list of new models / api clients / repos / DAOs / blocs}

---

## Synthesis

### Key Insight
### Recommended Approach
### Risks to Watch
### Open Questions
```

**Non-negotiables:**
- Every endpoint in §1 must have its own sub-section with SQL + request/response JSON + mapping table.
- No fabricated column names. Every column must appear in NANDEMO000/001 DDL or `remote-db-mapping.md`.
- Camelcase in JSON, UPPER_SNAKE_CASE in SQL.
- Errors are machine codes, not prose.
- End with Synthesis block (Key Insight / Recommended Approach / Risks / Open Questions).
