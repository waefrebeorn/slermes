# Slermes port workflow — references and skill data

Consolidated 2026-11-22. External claims carry the citation ids assigned by
`grounded-citations`; local claims come from this repository's gate logs and
the four parallel audits in `docs/dev/PARITY_MAPPING.md`.

## 1. Source ledger

| # | Source | What it contributes |
|---|---|---|
| [1] | https://arxiv.org/html/2606.27416 | Verifier-driven parallel agents; rules must be deterministic scripts, ~1% overhead |
| [2] | https://arxiv.org/html/2611.21084v1 | CompCert→Lean: 166 dependency units, definitions first, frozen interfaces, parallel proofs |
| [3] | https://arxiv.org/html/2606.03115v1 | SPOQ wave dispatch, dual validation gates, 1.4× on 2 slots, defects 0.34→0.20 |
| [4] | https://arxiv.org/html/2405.00796v1 | Bazel CI study: incremental median 4.22×/4.71× on long projects |
| [5] | https://bazel.build/remote/caching | Content-addressed action keys and cache algorithm |
| [6] | https://devops.com/your-ci-retries-are-hiding-real-bugs | Two retries kept 99.2% of buggy builds green |
| [7] | https://github.com/anvanvan/flows/blob/main/skills/strangler-fig-pattern/SKILL.md | Seam → characterization → abstraction → gradual routing |
| [8] | https://aidlc.io/legacy-code | Map, characterization tests, small reversible slices |
| [9] | https://code.claude.com/docs/en/workflows | Move orchestration into a rerunnable script; cross-check agents |
| [10] | https://arxiv.org/html/2604.07341v2 | Coverage-gap analysis against full fragment list; regenerate tests for uncovered functions |
| [11] | https://arxiv.org/html/2610.34633 | Fixed external oracle agents cannot edit; milestone cumulative checks; independent parity agent |
| [12] | https://arxiv.org/html/2610.30188v1 | Coverage-guided fuzzing, mutation testing, multiple port candidates |
| [13] | https://arxiv.org/html/2607.16024 | AST changed-function extraction + static call graph for test generation |
| [14] | https://arxiv.org/html/2609.15258v3 | Function-by-function differential test, then upstream differential, then validation |
| [15] | https://arxiv.org/html/2511.00160v1 | Migration context should include diffs between versions |
| [16] | https://arxiv.org/html/2602.09944 | Migration loop: plan → auto env → test validation → feedback refinement |
| [17] | https://arxiv.org/html/2607.28591v3 | Verified tasks require pass → fail → pass across three states |
| [18] | https://arxiv.org/html/2607.28271 | Locksmith Loop: input-state + stub-state fingerprints, parity-preserving mutation, repair the migrator |
| [19] | https://github.com/wallaceespindola/legacy-modernization-patterns | Pin behavior first including bugs; AI proposes, deterministic checks gate |
| [20] | https://snowmanlabs.com/insights/modernize-legacy-systems-without-big-bang-rewrite | Slice by bounded context; characterize before touching; agents bulk-generate tests |

### 1.9 Third research round: lock the seam, mutate to reach it

The Locksmith Loop records a **behavioral fingerprint** from an input-state
plus a stub-state — the paragraphs entered, branches taken, stub-operation log,
and terminal observable state — and compares it against the migrated target.[18]
That is exactly what our fixture `io` scripts plus emitted `trace` already
produce; what is missing is its explicit "Locked Paragraph" analysis: when
input-space search plateaus, a parity-preserving mutation should be used to
 penetrate the blocker rather than adding more of the same inputs. A parallel
migration workflow splits the loop into planning, environment, test-validation
and feedback-refinement agents, routing configuration errors to the environment
agent and semantic mismatches back to the translator.[16] Verified task
construction insists on pass → fail → pass across healthy, task and restored
states before admitting a case.[17] For legacy code, the discipline is to pin
current behavior **including bugs** with characterization tests, put an
abstraction in front of the legacy path, and let deterministic checks — not the
generating model — decide acceptance.[19][20]

## 2. Research → our workflow

| Research finding | Our implementation |
|---|---|
| Deterministic verifiers, not prose rules[1] | `run_one_oracle.sh`, `verify_build_health.py`, `check_port_wiring.py`, `check_gate_coverage.py` |
| Dependency-ordered units, frozen interfaces[2] | tier A→E file order; freeze C signatures after leaf gates are green |
| Wave dispatch + two gates[3] | dispatch files by tier wave; plan gate (seams identified) and evidence gate (oracle green) |
| Content-hash caching[4][5] | `tests/make_harness_archive.sh`; next step is per-harness `.key` files |
| Retries hide bugs[6] | retry sets `ORACLE_REBUILD=1`; retried gates must be reported as defects |
| Strangler slices[7][8] | `t_port_*.c` + `oracle_*.py` + production wrapper delegation |
| Rerunnable orchestration[9] | file-by-file driver in `docs/dev/FILE_BY_FILE_PORT_PLAN.md` |
| Coverage-gap repair loop[10] | `scripts/check_gate_coverage.py` compares fixture `func` set with LIVE AST functions |
| Fixed oracle + parity agent[11] | oracle AST-extracts LIVE bodies; `check_port_wiring.py` catches unwired ports |
| Differential before validation[14] | LIVE differential oracle first; build health and full suite second |
| AST + call graph[13] | flow-trace ledger records caller back-fixes |
| Diff-based context[15] | `docs/dev/PARITY_MAPPING.md` records quarry drift locations |

## 3. Skill data: non-negotiable rules

From `slermes-windows-port`, `writing-plans`, `grounded-citations` and this session:

1. **Behavior first, annotations last.** PoP comments and PORTED counters are not evidence.
2. **Fix the oracle/harness before the implementation** when they mismatch; verify twice.
3. **Emit input fields from C** via `json_dumps`; never retype them in the oracle.
4. **Do not normalise tokens that carry meaning.**
5. **Platform-guarded code hides syntax errors** from the only build you have.
6. **Hoist variables on platform branches** and avoid `goto` past initializers.
7. **One build at a time**; kill strays before starting another.
8. **Edit C with patch/Python file writes**, not shell heredocs; verify after append.
9. **Preserve `build/objects.mk` CRLF** or a one-line change becomes a 532-line rewrite.
10. **SAC/empty-stderr 127 is usually a crash or DLL issue**, not policy — bisect the fixture.
11. **UCRT API-set DLLs live in `/c/Windows/System32/downlevel`**; add to PATH.
12. **Archive linking picks duplicate definitions differently** — pin affected gates with `ARCHIVE_EXCLUDE`.
13. **Python `s[:n]` ends at a codepoint boundary**; C prefixes must consume continuations.
14. **Cache freshness must not scan `tests`** (19k fixture `.o` files); scan `src lib`.
15. **A retried gate is an infrastructure defect**, not a clean pass.
16. **Cite external claims at retrieval time**; generate Sources blocks from the ledger.
17. **Bite-sized tasks, exact paths, exact commands, frequent commits.**

## 4. Verified local facts

- Gate command: `bash tests/run_one_oracle.sh <name> oracle_<name>.py "" <deps>`.
- Full suite: `bash tests/run_all_oracles.sh`; `--changed` mode for waves.
- Wiring: `python scripts/check_port_wiring.py <port.c>`.
- Coverage: `python scripts/check_gate_coverage.py <oracle.py> <fixture.jsonl> <source.py>`.
- Health: `python tests/verify_build_health.py --no-link`.
- Measured: gate 6.8s→1.37s, 8-harness batch 20.3s→13.4s, full suite ~12min→230–289s.
- Suite18 254.08s, suite19 254.34s, suite20 835.47s (clean rebuild), suite22 464.72s, suite24 475.66s, suite25 493.14s, suite26 536.52s, suite27 526.62s, suite29 281.90s, suite33 585.98s, suite34 586.01s, suite35 599.96s, suite36 310.37s, suite37 314.25s, suite38 311.56s, suite39 313.68s, suite40 320.22s, suite41 319.24s, suite42 625.75s, suite43 636.36s, suite44 648.23s, suite45 658.32s, suite46 671.81s, suite47 362.06s, suite48 694.41s, suite49 702.86s, suite50 701.83s, suite51 711.71s, suite52 737.38s, suite53 728.45s, suite54 740.67s, suite55 743.98s, suite56 761.31s, suite57 770.53s, suite58 781.17s — 582→586 gates, with first-attempt defects now captured in `.testbin/oracle_fail_*.log`. Most single-gate retries were real borrower-after-free defects, not SAC infrastructure.
- Per-harness keys are only correct when object basenames resolve deterministically: sort candidates and prefer `src/` / `lib/` over fixture copies, otherwise keys silently flip and stale binaries are trusted.
- Registry: 13,622 Python functions ≥15 lines; 11,330 with no C reference.
- Known wired gap: `lib/libplugin/plugin.c:897-973` stubs on a real CLI path.
- Dead weight: 275 wired-dead PoP objects grandfathered by baseline.

## 5. File-by-file loop

1. AST-map the Python file; list every function, constants and imports.
2. Write inputs-only fixtures; oracle recomputes LIVE answers.
3. Port pure leaves first; gate to `N cases, 0 mismatches`.
4. Freeze signatures; port stateful callers.
5. Wire into a production caller; `check_port_wiring.py` must say `wired`.
6. Run coverage check; add fixtures for every uncovered LIVE function.
7. Flow-trace callers and back-fix; record in `docs/dev/flow_trace.jsonl`.
8. Commit the file slice; full suite at tier boundaries.

## 6. Improvement backlog derived from this document

- [x] Wiring guardrail (`check_port_wiring.py`)
- [x] Coverage-gap guardrail (`check_gate_coverage.py`)
- [x] Oracle mutation smoke (`scripts/check_oracle_mutation.py`) — pass→fail→pass
- [x] Flow-trace ledger (`docs/dev/flow_trace.jsonl`) — verified caller→callee fixes
- [x] Per-harness content-hash keys (`scripts/harness_key.py`): one `nm -A`
  pass over the archive builds a symbol index, each harness's reachable member
  set is hashed individually, and `make` relinks only changed gates.
- [x] Report retried gates in the full-suite summary; suite18 exposed
  `browser_tools_flow` and `skills_guard_flow` as first-attempt infra failures.
  Both now have a mitigation (explicit link pin + parallel SAC smoke).
- [ ] Pin remaining archive-sensitive gates via full-suite diff
- [ ] Wire the plugin registry stubs behind a real CLI oracle

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
[10] https://arxiv.org/html/2604.07341v2
[11] https://arxiv.org/html/2610.34633
[12] https://arxiv.org/html/2610.30188v1
[13] https://arxiv.org/html/2607.16024
[14] https://arxiv.org/html/2609.15258v3
[15] https://arxiv.org/html/2511.00160v1
[16] https://arxiv.org/html/2602.09944
[17] https://arxiv.org/html/2607.28591v3
[18] https://arxiv.org/html/2607.28271
[19] https://github.com/wallaceespindola/legacy-modernization-patterns
[20] https://snowmanlabs.com/insights/modernize-legacy-systems-without-big-bang-rewrite

## Native verification (2026-11-22)

`make -j8 slermes` with `source build/winlibs-env.sh`: rc=0, 19,096,667-byte `slermes.exe`.
`./slermes.exe --help` and `--version` both exit 0 when the UCRT downlevel DLL path from
`build/winlibs-env.sh` is active; without it the loader reports missing
`api-ms-win-crt-utility-l1-1-0.dll`, which is an environment issue, not a port regression.
`python tests/verify_build_health.py --no-link` reports ok after switching its shell to MSYS bash.

Serial mutation audit found no remaining oracle blind spots after fixing the transcription binary UAF; the remaining skips are harnesses that require the normal SLERMES_HOME layout and cannot run under an isolated HOME.
