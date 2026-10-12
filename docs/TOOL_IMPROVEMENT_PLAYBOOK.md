<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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

## Current state in slermes (measured this session)
- **511 `src/tools/*.c` files**, dispatched via `register_tool` (namespace-prefixed
  `plugin_register_tool`) in `src/agent/tool_executor*.c`.
- **112 tool files** hand-roll `json_parse`/`json_get` arg extraction — **no central
  schema-validation layer**. Each tool re-implements parsing; validation is inconsistent
  and absent by default.
- **No idempotency / retry-classification / dedupe** scaffolding in the core executor
  (only incidental "idempotent" comments in wrappers).
- **Trace exists** (`trace_upload` port of `build_trace_jsonl`, logging in
  `tool_executor`) but not as nested OTel-style spans — can't replay *why*.
- `tool_search_ports.c` + `mcp_tool.c` exist → the seed for on-demand discovery.

## The 6 improvements (ranked by ROI)

### P0 — Centralized tool-input validation  (highest ROI, lowest risk)
Add `tool_arg_schema_t` + `tool_validate_args` in `src/agent/tool_executor.c`. Each
tool declares expected keys/types/ranges/required. Executor rejects malformed calls
with a structured error **before** the body runs. Kills the 112-file duplicated-parse
problem and blocks injection-driven bad calls. *This is the first thing to build.*

### P1 — Error classification: retryable vs permanent
Replace ad-hoc `printf("error: ...")` returns with `tool_result_t { ok; retryable;
code; message; recovery_hint; }`. Executor branches on `retryable`; `recovery_hint`
tells the agent how to fix (Arcade Recovery Guide).

### P1 — Idempotency keys for side-effecting tools
Side-effecting tools accept `idem_key`; executor maintains a Hive-backed idem-key→result
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
