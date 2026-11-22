# File-by-file parity workflow: research-backed plan

> **For Hermes:** execute this plan one file at a time. Each file is a bounded
> strangler slice: characterize, port, verify, wire, trace back-fixes, commit.

**Goal:** close the largest real Python→C gaps one Python file at a time, with
behavior evidence and production wiring at every step, while keeping the
full suite fast enough to run at milestones.

**Architecture:** one Python source file per batch; one C port object per
batch; one LIVE-AST oracle per batch; one wiring check; one flow-trace pass
that back-fixes callers before the next file starts.

Citation note: bracketed ids cover claims taken from external research; all
uncited statements in sections 2–4 are local measurements from this repository
(gate logs, `parity_reality.py`, and the four parallel audits recorded in
`docs/dev/PARITY_MAPPING.md`).

---

## 1. What the research says about this workflow

### 1.1 Make the process executable, not prompt-encoded

Parallel coding agents fail by small instruction lapses compounding across
hundreds of tasks; the published remedy is deterministic verifier scripts that
refuse to advance when an artefact violates the spec, with the structural
machinery costing about 1% of wall-clock time.[1] Our equivalent is the gate
suite: `run_one_oracle.sh`, `check_port_wiring.py`, and the build-health gate.
Do not replace them with "please be careful" instructions to a subagent.

### 1.2 Partition by dependency, freeze interfaces, then parallelize

The six-week Rocq→Lean port of CompCert's verified core split the migration
into 166 dependency-ordered units, translated definitions first, reviewed them
en masse, then reconstructed proofs in parallel against **frozen statement
interfaces**.[2] Our analogue: extract the pure leaf functions of a Python
file, verify them against LIVE, freeze their C signatures, and only then
port the stateful callers. Eighty-eight percent of the expert's interactions
in that campaign concerned workflow/coordination, not technical direction —
so the leverage is in ordering and gating, not in smarter one-shot coding.[2]

### 1.3 Wave dispatch and dual validation gates

SPOQ computes parallel waves from a task DAG, applies a validation gate before
execution and another after, and reports 1.4× speedup on a two-slot local
backend with defect rate dropping from 0.34 to 0.20 per task under dual
gates.[3] On this box the DAG is the Python import/call graph; a wave is the
set of files whose helper dependencies are already frozen. Every file gets a
plan gate (are the LIVE functions and seams identified?) and an evidence gate
(oracle green + wiring + build health).

### 1.4 Strangler slices, not big-bang rewrite

The strangler-fig method requires a natural seam, characterization tests
before the new implementation, an abstraction layer that routes between old
and new, and a gradual traffic shift with output comparison at each step.[7]
Applied to brownfield code: build a reviewed map, pin behavior with
characterization tests, then change one small reversible slice at a time.[8]
That is exactly the `t_port_*.c` + `oracle_*.py` pair plus a wrapper that
calls the verified C body.

### 1.5 Content-hashed caching is the speed lever

Bazel modeling confirms incremental build is the dominant win: for long
projects the median speedup is 4.22× with a generic CI cache and 4.71× with a
tool-specific one, and actions are re-run only when a hash of command line,
environment, and input contents changes.[4][5] Our current mtime scan is the
equivalent of a coarse action key; per-harness content hashes are the next
step, not more parallelism.

### 1.6 Retries must be visible, not silent

A controlled experiment with two retries kept 99.2% of builds containing a
real intermittent bug green; flaky passes still carry an exit code of 0 by
default.[6] Our full-suite retry deliberately forces a relink, but it must
**report** which gates were retried. A retried gate is evidence of an
infrastructure defect, not a clean pass, and needs a tracked fix.

### 1.7 Codify the orchestration

Workflows that move the plan into a rerunnable script scale to hundreds of
subagents while keeping the conversation context small; they also allow
independent adversarial review of results before synthesis.[9] For this repo
that means a driver that reads a file list, runs the per-file gate sequence,
and writes results to disk rather than into chat.

---

## 2. Categorize gaps by Python file, not by feature counter

Measured baseline: 13,622 Python functions with bodies >=15 lines in the
seven core quarry trees; 11,330 have no C reference. The top unported clusters
by medium-function count are recorded in `docs/dev/PARITY_MAPPING.md`. The
useful categorization is by **slice risk and helper reuse**, not by count:

| Tier | Shape | Examples | First move |
|---|---|---|---|
| A. Pure leaf | string/path/scalar, no I/O | `hermes_cli/gateway_windows.py`, `hermes_cli/config.py`, `cron/jobs.py:rewrite_skill_refs` | AST oracle + C port in one batch (done for gateway_windows) |
| B. State-in-memory | dict/set registries, no OS side effects | `hermes_cli/kanban_db.py`, `agent/credential_pool.py`, `agent/agent_runtime_helpers.py` | differential oracle on a state mirror; then wire |
| C. Adapter protocol | normalise/encode/decode, network behind a seam | `gateway/platforms/base.py`, discord/telegram/feishu adapters | one shared adapter helper envelope, four files per wave |
| D. Orchestration | multi-step state machines, SQLite, processes | `gateway/run_turn*.py`, `gateway/run_*.py`, `persistence/sqlite/**` | sqlite differential oracles; definitions first, callers later |
| E. Product-only | registry/dispatcher wiring | `lib/libplugin/plugin.c` stubs, ~400 `*_flow*.c` doubles | seam hook + real caller; no new algorithm needed |

Tier A is where "file by file" pays fastest: a whole Python file of leaves is
verifiable in one gate, as the 7-function `gateway_windows.py` batch proved.
Tier E is where existing green oracles become product behavior; it must not be
counted as *new* parity until a caller is shown by `check_port_wiring.py`.

### The next ten files (in dependency order)

1. `hermes_cli/config.py` — schema validation and config-set coercion; tier A.
2. `cron/jobs.py` — `rewrite_skill_refs` plus job state helpers; tier A→B.
3. `gateway/platforms/base.py` — media path/extraction helpers; tier A→C.
4. `hermes_cli/profiles.py` — alias map and profile metadata; tier A→B.
5. `agent/credential_pool.py` — pure pool-row and match helpers; tier B.
6. `hermes_cli/kanban_db.py` — task state transitions; tier B, sqlite mirror.
7. `hermes_cli/update_cmd_windows.py` — Windows service/process helpers; tier B.
8. `gateway/run_busy.py` — busy-ack and queue decisions; tier C→D.
9. `agent/turn_recovery.py` — classification routing; tier D.
10. `lib/libplugin/plugin.c` — wire the registry stubs; tier E (no new oracle
    credit until the CLI path is exercised).

---

## 3. The per-file loop (bite-sized, 2–5 minutes per step)

For Python file `P` with target functions `F1..Fn`:

1. **Map.** AST-walk `P`; list `F1..Fn` with line counts, module constants and
   every in-function import. Write them to
   `docs/dev/file_slices/<module>.md`. Gate: no function is skipped silently.
2. **Characterize.** Create `tests/fixtures/<gate>.jsonl` with inputs only.
   The oracle recomputes the LIVE answer; never store C-derived expectations.
3. **Oracle.** `tests/oracle_<gate>.py` AST-extracts `F1..Fn` and constants,
   strips decorators, seeds only needed builtins, projects exceptions as
   `{"__raise__":..., "message":...}`. Fail closed on missing defs or 0 cases.
4. **Port.** `src/<area>/<gate>.c` implements the same signatures with the
   same seam boundary. Register the object in `build/objects.mk` (preserve
   CRLF!) and the gate in `tests/run_all_oracles.sh` DEPS.
5. **Verify.** `bash tests/run_one_oracle.sh <gate> oracle_<gate>.py "" <deps>`
   → `ORACLE <gate>: N cases, 0 mismatches`.
6. **Wire.** `python scripts/check_port_wiring.py <port.c>` must say `wired`.
   If it says harness-only, route one existing production wrapper through the
   verified body before claiming parity, as was done for `windows.c`.
7. **Flow-trace back-fix.** Enumerate every caller of the ported functions;
   run each affected gate with `--changed`, and fix mismatches in callers or
   harness seams before starting the next Python file.
8. **Commit** the whole file slice with the gate count in the message, then
   push. Milestone: full suite with the forced-relink retry enabled.

### Flow-trace ledger

Keep `docs/dev/flow_trace.jsonl` with one line per back-fix:
`{"file": P, "caller": C, "symptom": ..., "fix": ..., "gate": G, "sha": ...}`.
This is the record that distinguishes "ported" from "integrated" and prevents
the next file's batch from reintroducing an old divergence.

---

## 4. Speed roadmap after the file batches

1. Per-harness content-hash keys over harness source bytes, its quoted-include
   closure, linked object bytes and flags; store `.testbin/harnesses/<h>.key`
   and skip unchanged targets.[4][5]
2. Report retried gates explicitly in the full-suite summary; treat the retry
   count as a defect metric.[6]
3. Keep `HARNESS_JOBS` at 8–10; measured gains above that are ~10% on cold
   builds and zero once content hashing lands.
4. Run `--changed` after every file; full suite only at the end of each tier.

## Sources

[1] https://arxiv.org/html/2606.27416
[2] https://arxiv.org/html/2611.21084v1
[3] https://arxiv.org/html/2606.03115v1
[4] https://arxiv.org/html/2405.00796v1
[5] https://bazel.build/remote/caching
[6] https://devops.com/your-ci-retries-are-hiding-real-bugs
[7] https://github.com/anvanvan/flows/blob/main/skills/strangler-fig-pattern/SKILL.md
[8] https://aidlc.io/legacy-code
[9] https://code.claude.com/docs/en/workflows
