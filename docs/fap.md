# FAP — Functional Alignment Problem

**One term, one meaning, used everywhere.** A FAP is a *behavioral divergence*
between the slermes C implementation and **LIVE Python Hermes** that the
function-level parity scanner (`slermes_parity_battleground.py`) **cannot see**.

## Why a separate term exists

The parity scanner reports four static classes:

| Class | What it means | Can it catch a *wrong* implementation? |
|-------|---------------|----------------------------------------|
| `PORTED` | C fn exists + has a `/* PoP: */` annotation | No |
| `REAL_GAP` | Python fn has **no** C implementation yet | No (it's missing, not wrong) |
| `PARTIAL` | C fn exists but is incomplete | No |
| `STUB` | C fn is a façade (const return / no-op) | Only if it's a *shaped* façade |

A C function can be `PORTED` (correct annotation, compiles, passes the
depth-check) and **still produce output that differs from LIVE Python** — wrong
JSON quoting, different key order, a dropped provider from a registry table, a
retry-state set that diverges, an env-gated value computed incorrectly. The
scanner is blind to all of these because it never *executes* anything against
the live Python.

**That blind spot is exactly what a FAP is.** When the oracle harness runs the
C harness and the LIVE Python oracle over the same fixture and the outputs
differ, that diff **is a FAP**.

## Definition (canonical)

> **FAP (Functional Alignment Problem):** a C implementation that is
> statically "ported" (annotated, compiles, depth-check clean) but whose
> *runtime behavior* on concrete inputs diverges from LIVE Python Hermes.
> Detected only by the oracle harness (`tests/oracle/`): C output ≠ Python
> output for the same fixture.

Synonyms that MUST NOT be used to avoid confusion: *drift, desync, divergence,
defect, behavioral gap, mismatch-as-a-bug-in-commit-notes*. Call it a **FAP**.

## Two sub-types

1. **Real FAP** — the C code genuinely behaves differently from LIVE Python and
   should be fixed (e.g. C provider-auth table ≠ Python `PROVIDER_REGISTRY`
   membership; C json serialization differs in key order/quoting).
2. **False FAP** — the oracle reports a diff but it is an *environment artifact*,
   not a code bug. The canonical example: `coding_context` embeds live
   `git status` (e.g. `Status: 8 modified`) into its output, so the C and
   Python sides report different numbers on different runs. That is NOT a FAP —
   it is environmental noise the oracle's `normalize` config must strip. The
   rule: **if the divergence is caused by non-deterministic environment state
   (git working tree, `TERM`, `NO_COLOR`, clock, hostname), it is a FALSE FAP —
   fix the oracle normalization, not the C code.**

Also distinct from `REAL_GAP`: a missing `port_web_git.h` header is a build/missing-
implementation problem (scanner-visible once wired), **not** a FAP. FAPs only
exist for functions that *are* implemented and *do* run.

## How to find FAPs (the only correct procedure)

The oracle harness is the FAP detector. It is the project's behavioral safety
net; keeping it wired is what makes FAPs *visible* instead of silent.

```bash
# Run ALL registered oracles (each diffs C vs LIVE Python over fixtures):
bash tests/oracle/run_oracles.sh            # CI mode: exit 1 on any FAP

# Run a single port:
bash tests/oracle/run_oracles.sh <port_name>

# Inspect one harness directly:
bash tests/oracle/runners/run_oracle.sh <port_name>
```

Any `cases: MISMATCH` line from `run_oracles.sh` is a **FAP candidate**. Triage:

1. Re-run both sides locally and diff the raw outputs.
2. If the diff is environment state (git status, hostname, clock, `TERM`) →
   **FALSE FAP**: add a `strip_patterns`/`strip_fields` entry to
   `tests/oracle/registry.json`'s `normalize` block, do NOT touch C.
3. If the diff is real behavior → **Real FAP**: read the LIVE Python source,
   fix the C, re-run until `MATCH`.

## Current known FAPs (from the v622+ oracle sweep)

| Port | Type | Summary |
|------|------|---------|
| `provider_auth` | Real FAP | C provider-auth table ≠ LIVE Python `PROVIDER_REGISTRY` membership/order |
| `usage_pricing` | Real FAP | C json serialization diverges from Python (key order / quoting) |
| `turn_retry_state` | Real FAP | C retry-attempt state set differs from Python |
| `coding_context` | False FAP | live `git status` embedded in output → normalize, not a code bug |

These were **silent** before the oracle registry was restored to cover all 89
runnable ports (it had regressed to 2). A FAP that never runs is still a FAP —
it just isn't *found* until the harness exercises it.

## The one rule

If you change C code that has an oracle, **run the oracle**. If you add a port,
**register it in `tests/oracle/registry.json`** so its FAPs can be found. A
`PORTED` count of 11,500+ with 0 `REAL_GAP` tells you nothing about FAPs — only
the oracle does.
