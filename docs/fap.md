<!-- PARITY:AUTO -->| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# FAP — Functional Alignment Problem

**One term, one meaning, used everywhere.** A FAP is a *behavioral divergence*
between the slermes C implementation and **LIVE Python Hermes** that the
function-level parity scanner (`slermes_parity_battleground.py`) **cannot see**.

## Why a separate term exists

The parity scanner reports static classes:

| Class | What it means | Can it catch a *wrong* implementation? |
|-------|---------------|----------------------------------------|
| `PORTED` | C fn exists + has a `/* PoP: */` annotation | No |
| `REAL_GAP` | Python fn has **no** C implementation yet | No (it's missing, not wrong) |
| `PARTIAL` | C fn exists but is incomplete | No |

A C function can be `PORTED` (correct annotation, compiles, linked) and
**still produce output that differs from LIVE Python** — wrong JSON quoting,
different key order, a dropped provider from a registry table, a retry-state
set that diverges, an env-gated value computed incorrectly. The scanner is
blind to all of these because it never *executes* anything against the live
Python.

**That blind spot is exactly what a FAP is.** When the oracle harness runs
the C harness and the LIVE Python oracle over the same fixture and the outputs
differ, that diff **is a FAP candidate**.

## Definition (canonical)

> **FAP (Functional Alignment Problem):** a C implementation that is
> statically "ported" (annotated, compiles, linked) but whose *runtime
> behavior* on concrete inputs diverges from LIVE Python Hermes. Detected
> only by the oracle harness: C output ≠ Python output for the same fixture.

Synonyms that MUST NOT be used: *drift, desync, divergence, defect,
behavioral gap*. Call it a **FAP**.

## Two sub-types

1. **Real FAP** — the C code genuinely behaves differently from LIVE Python
   and should be fixed (e.g. a provider-auth table missing entries; a host
   matcher implemented as `strstr` instead of exact hostname comparison).
2. **False FAP** — the oracle reports a diff but it is an *artifact*, not a
   code bug. Classes seen in this repo:
   - **Environment noise**: non-deterministic state (git working tree, `TERM`,
     clock, hostname) leaks into the output. Fix the oracle's normalization.
   - **Stale copied contract**: the oracle copies a Python helper instead of
     importing it, and upstream changed the helper (e.g. `utils._hostname_of`
     gained `.rstrip(".")`; the copy without it flagged trailing-dot hosts).
     Re-align the copy to LIVE source, or import the live function.
   - **Shape mismatch**: `0` vs `0.0`, compact vs spaced JSON, key order,
     `str(Path)` backslashes on Windows vs `/` in C. Normalize at the
     boundary (see `oracle_pm_environments.py`'s `norm()` convention).
   - **Harness/runner bug**: wrong object on the link, stub with the wrong
     signature, missing include root, a dispatch table that calls a function
     *name string* instead of the function. The port is innocent.

Also distinct from `REAL_GAP`: a missing implementation is scanner-visible
work, **not** a FAP. FAPs only exist for functions that *are* implemented and
*do* run.

## How to find FAPs (the only correct procedure)

```bash
# ALL committed oracles (the standing regression gate):
bash tests/run_all_oracles.sh            # ORACLE_JOBS=1 on MSYS2

# One port:
bash tests/run_one_oracle.sh <name> <oracle.py> "" "<obj> [<obj> ...]"
```

Any `MISMATCH` line is a **FAP candidate**. Triage:

1. Re-run both sides and diff the raw outputs.
2. Environment/shape/stale-copy artifact → **False FAP**: fix the oracle
   (normalization, aligned contract, correct DEPS), do NOT touch C.
3. Real behavior difference → **Real FAP**: read the LIVE Python source,
   fix the C, re-run until MATCH.

**A `PYERR` line is never a pass.** An oracle whose Python side raises on
every case still prints "0 mismatches" — that is a broken oracle, not a
green port. The auto-generated dispatch oracles had exactly this bug (the
ARGS table stores function names; the call site invoked the string) across
57 files; fixed at the latest sweep in `gen_oracle.py` and every affected oracle.
When a suite reports "0 mismatches", check that cases actually computed
expected values.

## Recently closed FAPs (latest sweep)

| Port | Type | Resolution |
|------|------|------------|
| `left_core_migration` | False FAP (stale copy) + runner | Oracle's `_bh` copy lacked LIVE `_hostname_of`'s `.lower().rstrip(".")`; DEPS pointed at an unbuilt SAC-blocked object instead of `provider_registry.o`. Aligned copy to live `utils.py`, fixed DEPS + dependency stub. 153 cases MATCH. |
| `update_pause_record` | False FAP (shape) + oracle bug | `str(Path)` backslashes vs C `/` — normalized both sides; NULL-root case now expects the C contract (empty key). SAC-safe `-O0` hash image for the link. 211 cases MATCH. |
| `shared_metrics_norm` | Runner bug | DEPS named the wrong twin path; harness needs both contract objects + `smc_event_extractors.o`. 127 cases MATCH. |
| `pm_environments` | Runner bug + dead port | Port was excused/unlinked; registered it, SAC-safe `-O0` hash image. 6 cases MATCH. |
| provider registry (binary-wide) | **Real FAP** | `infrastructure_stubs.c` shadowed 12 `provider_*` functions with wrong-shape facades (`strstr` host matching, int-for-char* hostname, 4-entry table for `catalog_provider_t` readers). Removed stubs, linked the real `provider_registry.c` port. |
| secret scope (binary-wide) | **Real FAP** | Wrong-signature stubs shadowed the real `port_agent_secret_scope.c`; `auxc_scoped_key_env` freed a borrowed pointer. Linked the real port, fixed the caller, synced the global-env allowlist to live Python. |

Historical FAP closures (provider_auth table, turn_retry_state field,
usage_pricing include path, coding_context git-status normalization,
web_git include roots) are recorded in git history; the gates that pin them
stay green.

## The one rule

If you change C code that has an oracle, **run the oracle**. If you add a
port, **register its oracle** so its FAPs can be found. A PORTED count tells
you nothing about FAPs — only the oracle does. And a green oracle total is
meaningless unless the cases actually ran: "0 cases" is a link failure in
disguise, and a suite that cannot build is a failure, not a skip.

## The deep gate: water-cup trace-cups

The output-diff oracle has one blind spot it can never cover: a C function
that returns the *right* value by executing the *wrong* control flow passes
every fixture. That class is what the **water-cup** (`tests/watercup/`, see
[watercup-parity-oracle.md](watercup-parity-oracle.md)) is for — it diffs the
*traced branch/loop/recursion ladder*, not just outputs.

Run both:

```bash
bash tests/run_all_oracles.sh           # diff oracle (breadth)
python tests/watercup/wc_run.py         # water-cup deep gate (path parity)
```

A port is parity-closed only when BOTH say MATCH.
