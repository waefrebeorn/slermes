# Slermes — Work Dashboard (Single Source of Truth)

> **This is the only doc you read when you walk into a room.** Everything else is
> detail. It reconciles the project's real detectors into one actionable plan and
> is regenerated/updated as work closes. Last updated: **2026-08-19** (session:
> water-cup project-wide + detector hardening + FAP closure).
>
> **Orientation rule:** trust the numbers in §1 (live scanner + water-cup gates),
> not the auto-generated prose in `parity-summary.md` / `GAP_LIST.md` — those
> were found to be stale/contradictory and have been moved to `docs/archive/`.

## §1 — Ground truth (what the detectors actually say RIGHT NOW)

### 1A. Function-level parity (live scanner `live_parity_scan.json`, 2026-08-19)
| Class | Count | % | Meaning |
|-------|-------|---|---------|
| PORTED | 13,581 | 85.2% | C11 + PoP annotation + real body |
| REAL_GAP | 2,329 | 14.6% | not yet ported |
| ├ annotation-only stubs (DA-4) | 473 | — | PoP comment, NO real C body (scanner-fooling) |
| └ genuine gaps (no C at all) | 1,856 | — | nothing in the tree |
| PARTIAL | 32 | 0.2% | C fn present, annotation missing |
| BOOTLEG (echo stubs) | 0 | — | no-work stubs (recursive_false_gap_hunter) |
| **TOTAL** | **15,942** | 100% | all Python fns scanned |

### 1B. Water-cup deep-gate (behavioral, language-agnostic — `tests/watercup/`)
Runs in CI via `watercup_all` port. A port is parity-closed only when BOTH the
diff oracle AND the water-cup say MATCH.

| Gate | Subsystem | Status | What it guards |
|------|-----------|--------|----------------|
| `web_fs_list` | web | ✅ GREEN | decision-structure cup (I/O abstracted) matches LIVE Python |
| `usage_pricing_format_cost_label` | agent | ✅ GREEN | sub-cent rounding (ROUND_HALF_EVEN) matches Python `Decimal` |
| `model_normalize_detect_vendor` | hermes_cli | ✅ GREEN | empty-string guard matches Python falsy semantics |
| `subscription_view_format_tier_row` | agent | ✅ GREEN | thousands-grouped money + credits suffix matches LIVE |
| `registry_validate_args` | tools | ✅ GREEN | P0 centralized input validation — rejects bad/type-mismatched args before dispatch |
| `watercup_web_auth` | web | ✅ GREEN | `is_public_path` now == LIVE `PUBLIC_API_PATHS` (8 paths, 1:1 parity) |
| `watercup_json_escape` | web | ✅ GREEN | `web_json_escape` now escapes `<0x20` like LIVE `json.dumps` (`\b`/`\f` short, else `\u00XX`) |
| `tool_result_classification` | tools | ✅ GREEN | P1: classifies tool errors retryable/permanent (fail-closed default) + `tool_error_with_class` tag |
| `tool_idempotency` | tools | ✅ GREEN | P1: omap-backed idempotency cache — retry returns cached result (no double side effect) |

> **9 project-wide gates GREEN** (incl. `subscription_view_format_tier_row`, `registry_validate_args`, `watercup_web_auth`, `watercup_json_escape`, `tool_result_classification`, `tool_idempotency` added this session).
> Candidate pool for more gates: **3,042 pure functions** with both a C port and a LIVE
> Python twin (from the PoP-annotation cross-reference). Only 9 are currently gated.

### 1C. PoP-integrity audit (`scripts/audit_ports.py` v2 — hardened this session)
v1 reported ~17,500 false-positive "orphans" (broken regex). v2 scans definitions
**and** declarations:
| Issue | Count | Action |
|-------|-------|--------|
| TRUE_ORPHAN (C fn credited but absent) | see `audit_ports.py --json` (~6,431, over-inclusive) | re-port or fix PoP credit |
| MULTILINE_POP (scanner-blind comment) | 255 | fold onto one line |
| DUP_CREDIT (C fn → 2+ python twins) | 751 | disambiguate names |
| STUB_MARKER (TODO/FIXME in port body) | 17 | implement |

> **Note:** the TRUE_ORPHAN count is over-inclusive — it includes legitimate
> namespace renames (`register_tool`→`plugin_register_tool`, permitted by the v2.2
> name-parity protocol) and detector regex gaps. The authoritative parity picture is
> the live scanner (§1A) + the **3,042 pure-ish twin functions** (§1D).

### 1D. File-level organization parity (the "match the filesystem" gap)
The C tree does NOT mirror the Python package tree 1:1. Of **523 Python modules**
(agent/, hermes_cli/, tools/, gateway/, cron/):
- **318** have a same-named C file (`usage_pricing.py`→`usage_pricing.c`).
- **205** have NO same-named C file — their code lives under a different filename
  (`hermes_cli/commands.py`→`src/cli/commands.c`; `agent/subscription_view.py`→`src/agent/ports_wrappers.c`).

> **Reorg plan (per `slermes-name-parity` skill Phases 1 & 3):** bring the C tree to
> 1:1 with Python via (a) org-parity relocation `src/ports/<pkg>/X.c`→`src/<pkg>/X.c`
> and (b) file-level thin-wrapper `.c` files for modules whose real code lives elsewhere.
> Execute in TWO git-checkpointed phases (mechanical rename, then collisions) — never one
> blind pass.

## §2 — The work, ranked (this is the to-do)

### P0 — FAPs the water-cup already caught (real, verified, fixable)
1. **`watercup_web_auth` (SHIPPED ✅)** — `src/web_dashboard.c::is_public_path()`
   now matches LIVE `PUBLIC_API_PATHS` exactly (8 paths). Removed 4 spurious
   C-only entries (`/health`, `/api/version`, `/api/token`, `/api/platforms`);
   added 6 LIVE entries (`/api/config/defaults`, `/api/config/schema`,
   `/api/model/info`, `/api/dashboard/themes`, `/api/dashboard/plugins`,
   `/api/cron/fire`). Proven by `tests/watercup/gates/watercup_web_auth/`
   (harness + LIVE-Python twin, both PASS).
3. **`watercup_json_escape` (SHIPPED ✅)** — `src/web_app.c::web_json_escape` now
   escapes control chars `< 0x20` like LIVE `json.dumps`: `\b`/`\f` short
   escapes, everything else `< 0x20` as `\u00XX`. Previously emitted raw
   control bytes (invalid JSON). Proven by
   `tests/watercup/gates/watercup_json_escape/` (harness + LIVE twin, both PASS).

3b. **Tool input validation gate (SHIPPED ✅)** — `src/ports/tools/registry.c::
    validate_args_json` now runs BEFORE `coerce_args_json`/handler on both dispatch
    paths (main + repaired) and rejects non-object args, type-mismatched present
    args (string/integer/number/boolean/array/object + union `type` arrays), and
    missing `required` args — returning a standardized `tool_error` JSON with
    `retryable:false` + a recovery hint (no handler side effects on bad input).
    Proven by `tests/watercup/gates/registry_validate_args/verify.c` (9 assertions,
    ALL PASS against real libjson). **Bonus:** building it root-caused + fixed two
    LATENT libjson bugs in the existing `coerce_args_json` — `json_len()` (array
    length) was used where `json_object_size()` (object count) was needed, and
    `json_get(obj, index)` (array accessor) was used where `json_object_get_at()`
    was needed — so coercion was silently a no-op before; it now actually coerces.

### P1 — Tool-layer reliability (SHIPPED ✅, research-backed)
Built the two structural fixes the agent-tooling research identified as the
"retries are structurally broken" gap:

- **Error classification** — `src/tools/tool_result_classification.c` (was a
  name-parity STUB; now REAL). `tool_classify_error()` returns
  `NONE/TRANSIENT/PERMANENT`: explicit `"retryable"` field wins; else keyword
  heuristics on message+code+status; **default PERMANENT (fail-closed)** so the
  agent loop never blindly retries an unknown error. `tool_error_with_class()`
  non-destructively appends `_retry_class` to an error result so the loop can
  decide. Proven by `tests/watercup/gates/tool_result_classification/`.
- **Idempotency keys** — `src/tools/tool_idempotency.c` + `include/tool_idempotency.h`,
  omap-backed (no landlocked static). `tool_idem_key(tool,args)` (explicit
  `idempotency_key` arg overrides); `tool_idem_put/get` with TTL. Wired into
  `registry_dispatch` (both main + repaired paths): lookup-before-exec returns
  the cached result on retry WITHOUT re-running the handler (no double send), and
  successful results are stored. `tool_registry_shutdown()` frees the cache.
  Proven by `tests/watercup/gates/tool_idempotency/`.

These give the agent a real retry signal (transient vs permanent) and make
retries safe (idempotent) — the two missing primitives from the research.

### P1 — Cheat-surface closure (the "173 missing-from-index" class)
3. Resolve the 473 DA-4 annotation-only stubs (PoP comment, no body) — give them
   real C or drop the false credit.
4. Resolve the 751 DUP_CREDIT collisions — each C fn must credit exactly one
   Python twin (name-parity).
5. Fold the 255 MULTILINE_POP comments onto one line so the scanner can read them.

### P2 — Breadth (the 2,329 REAL_GAPs + water-cup expansion)
6. Expand water-cup coverage across the **3,042 pure twin functions** (§1D). Each gate
   is 3 files (`gate.json` + `harness.c` + `twin.py`); `wc_run.py` builds+runs+diffs
   automatically. Priority: error_classifier, rate_limit_tracker, subscription_view,
   model_normalize, usage_pricing, kanban_db, auth, config.
7. Close the 2,329 REAL_GAPs prioritized by subsystem usage.

### P3 — File organization parity ("match the filesystem")
8. Execute the `slermes-name-parity` Phase 1 (mechanical rename + org-parity relocation)
   in TWO git-checkpointed passes. Target: C tree mirrors Python package tree 1:1
   (close the 205/523 file-name gap from §1D). Collisions resolved per the skill's
   symbol-aware-merge reference — never blind bulk-merge (drops load-bearing code).
9. Add file-level thin-wrapper `.c` files for modules whose real code lives elsewhere.

### P4 — Hygiene
10. Keep `audit_ports.py` clean; run `make parity-walkway` to refresh §1A. Water-cup
    `watercup_all` gate must stay GREEN in CI.

## §3 — How to find work when you walk in

```
1. make parity-walkway          # refresh live_parity_scan.json (§1A)
2. python3 scripts/audit_ports.py --json   # refresh PoP-integrity (§1C)
3. bash tests/oracle/run_watercup_all.sh   # refresh water-cup gates (§1B)
4. Read §2 of THIS file. Do the top P0 item. Close it. Update §1B.
```

## §4 — Architecture of the audit tooling (so you don't reinvent it)
- **Diff oracle** (`tests/oracle/`): output-vector equality. Coarse gate.
- **Water-cup** (`tests/watercup/`): behavioral trace-cup (B/L/R/r/V → FNV-1a).
  Deep gate; language-agnostic; catches "right output, wrong path".
  - `wc_run.py` = generic engine. `gates/<name>/` = {gate.json, harness.c, twin.py}.
  - Add a gate: `mkdir gates/<fn>` + 3 files. No runner changes needed.
- **PoP audit** (`scripts/audit_ports.py`): flags orphan/multiline/dup/stub PoPs.
- **Gap scanner** (`tests/slermes_parity_battleground.py`): function-level parity.

> **Stale/contradictory docs** (`parity-summary.md`, `GAP_LIST.md`,
> `real-gap-list.md` with over-optimistic claims) are archived under
> `docs/archive/` — don't trust their prose; trust this dashboard + the live JSON.
