<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Tool Improvement Playbook (research-backed, )

> How to make the slermes C11 tool layer *better* — synthesized from 2026 agent-tool
> research and the project's own `AGENTS.md` design philosophy (narrow-waist core,
> Footprint Ladder). Companion skill: `slermes-tool-improvement`.
>
> **Key reframe from research:** "better tools" ≠ "more tools." The wins are in the
> *harness around tools* — validation, error classification, idempotency, discovery,
> observability — not in adding surface.

## Research sources (what the field says in 2026)
- **MLflow practitioner guide** — "if >8 tools registered at once, you have a design
  problem"; narrow job-specific tools + typed schemas outperform broad wrappers;
  put validation/permission/retry in the *harness*, not the prompt; set hard budgets.
- **Arcade 52 tool patterns** — Constrained Input, Idempotent Operation, Timeout
  Boundary, Error Classification (retryable vs permanent), Recovery Guide, Permission
  Gate (code not prompt), Audit Trail, Tool Registry, Token-Efficient Response.
- **TianPan idempotency (2026)** — agent retries are structurally broken because the
  framework retries at the LLM layer with no record of committed side effects; fix =
  idempotency keys in the tool wrapper, results cached by key, survive restarts.
- **OTel GenAI conventions (2026)** — span-per-tool-invocation with name+version, input
  args, raw output, latency, success/failure, side-effect flag; thread `trace_id`
  through sub-agent/delegate hops. "If you cannot explain why the agent did something,
  you do not control it."
- **Zylos / MCP code-execution** — load tool *definitions* on demand (hierarchical
  selection), not all upfront; cuts context noise + selection errors.

## Current state in slermes (re-measured live)
- **357 `src/tools/*.c` files**, dispatched via `registry_dispatch`
  (`src/tools/registry.c`); plugins register through `plugin_register_tool`.
- **P0 validation SHIPPED**: `validate_args_json` runs BEFORE coercion and
  the handler on both dispatch paths (main + repaired); rejects non-object
  args, type mismatches, missing required keys with a structured
  `tool_error` (`retryable:false` + recovery hint). Pinned by the
  `registry_validate_args` watercup gate.
- **P1 error classification SHIPPED**: `tool_result_classification.c` —
  `tool_classify_error` returns NONE/TRANSIENT/PERMANENT, explicit
  `retryable` field wins, **default PERMANENT (fail-closed)**;
  `tool_error_with_class` tags results. Watercup-gated.
- **P1 idempotency SHIPPED**: `tool_idempotency.c` — omap-backed cache,
  `tool_idem_key/put/get` with TTL, wired into `registry_dispatch`
  (lookup-before-exec, no double side effect), restart-survival via
  `tool_idem_attach` (JSON under HERMES_HOME). Watercup-gated
  (`tool_idempotency`, `tool_idempotency_persist`).
- **Loop consumption SHIPPED**: `conversation_loop.c` `dispatch_with_retry`
  retries TRANSIENT up to TOOL_RETRY_MAX with backoff; safe because the
  registry is idempotent. Watercup-gated (`tool_dispatch_retry`).
- **Remaining**: P2 on-demand discovery, P2 OTel-style nested spans, P3
  output shaping (below). Trace exists (`trace_upload`) but not as nested
  spans — can't yet replay *why*.

## The improvements (P0/P1 shipped; the rest ranked by ROI)

### P0 — Centralized tool-input validation  (SHIPPED — registry_validate_args gate)
Implemented as `validate_args_json` in `src/tools/registry.c` (runs before
coerce + handler on both dispatch paths). Each tool's `schema_json` declares
expected keys/types/required; malformed calls get a structured error
**before** the body runs. The original proposal:
tool declares expected keys/types/ranges/required. Executor rejects malformed calls
with a structured error **before** the body runs. Kills the 112-file duplicated-parse
problem and blocks injection-driven bad calls. *This is the first thing to build.*

### P1 — Error classification: retryable vs permanent  (SHIPPED — watercup-gated)
Implemented in `src/tools/tool_result_classification.c`
(`tool_classify_error`, fail-closed PERMANENT default, `tool_error_with_class`).
The original proposal:
code; message; recovery_hint; }`. Executor branches on `retryable`; `recovery_hint`
tells the agent how to fix (Arcade Recovery Guide).

### P1 — Idempotency keys for side-effecting tools  (SHIPPED — watercup-gated)
Implemented in `src/tools/tool_idempotency.c`, wired into `registry_dispatch`
with restart-survival (`tool_idem_attach`). The original proposal:
cache (survives restart via session DB). Prevents double-send/double-write on timeout
retry. Directly addresses TianPan's "structurally broken retries."

### P2 — Narrow tools + on-demand discovery
Tag tools by capability; expose `discover_tools(capability)` so only relevant schemas
enter context (extend `tool_search_ports.c`/`mcp_tool.c`). Cuts footprint + selection
errors vs dumping all 500+ schemas every call.

### P2 — Standardized observability (OTel-style spans)
Wrap every tool invocation as a `tool_span_t` (name+version, args, output, latency,
success, side-effect flag); thread `trace_id` through `async_delegation`. Feed existing
`trace_upload`.

### P3 — Output shaping / token efficiency
Add `detail` arg; cap default output size (Arcade Token-Efficient Response).

## Execution rules (from AGENTS.md + research)
- Implement at the **executor layer** (shared), not per-tool — *extend, don't duplicate*.
- Never add core tools to solve a harness gap (terminal+file already cover most).
- Permission/validation must be **enforced in code**, not prompts.
- After each change: `make slermes` green + `run_watercup_all.sh` green.
- Behavior-contract (invariant) tests, not change-detector tests.
