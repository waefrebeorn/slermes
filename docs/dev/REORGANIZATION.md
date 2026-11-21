# Reorganization — port work that fits the repo

Decision record, 2026-11-21. Trigger: four waves of adapter "remainder"
flows had been landed with green leaf oracles while the repo's own scoreboards
showed the main fronts untouched. The work was optimized for the scanner, not
for the binary. This document re-anchors the loop.

## Measured evidence (live tools, not memory)

- `tests/parity_reality.py`: 38,978 Python features — 16,731 implemented
  (42.9%), **398 placeholders**, **21,849 missing entirely (56.1%)**.
  Biggest holes: hermes_cli 9,259, gateway 4,525, tools 3,850, agent 3,815.
- `README.md` sentinel: watercup **58 PASS / 1,685 (3.4%)**. The repo's own
  definition of done requires the differential oracle *and* a watercup gate
  where one exists, plus build/link.
- `tests/check_dead_ports.py` (new): 2,061 PoP-tagged C files; **283 have
  only static PoP functions and no external entry point — 264 of them are
  wired into `build/objects.mk`**. All 264 have `.o` files on disk and
  `nm -g --defined-only` reports **zero exported symbols on all 264** —
  binary-level confirmation, not just a source heuristic. The recent
  adapter-flow objects were individually verified the same way.
- No `slermes.exe` exists in the tree; full-link is SAC-blocked. Source ≠
  binary is not a theoretical concern here.
- `docs/dev/devils-worklist.md` already named the priorities: false-green
  reporting, stub clusters, DA-5 built-link. The adapter-remainder grind was
  not on that list.

## Diagnosis

The loop had drifted to "pick the next unclaimed def in an adapter, write a
bespoke flow + fixtures, get a green isolated-function oracle, wire the
object, commit." Each step is locally checkable and globally irrelevant:
static functions with no callers, no watercup gate, no binary. Falling gap
counts were reported as progress — the exact failure mode the user's standing
correction and the repo mantra both forbid.

## New loop (in order)

1. **Start from `tests/parity_reality.py`, not the scanner.** Scanner PORTED
   is a pointer; body-level reality and watercup are the verdict.
2. **Pick work from the devil's worklist / placeholder clusters**, largest
   area first: hermes_cli (159 placeholders), gateway (97), tools (63),
   agent (62), cron (17). Re-port in place — replace the stub body, keep the
   existing PoP claim honest. Do not add another parallel dead flow file.
3. **Every port needs a caller.** Before wiring, prove with
   `nm -g --defined-only <obj>` that the function is externally reachable, or
   wire the dispatcher that calls it. A static-only port is not done.
4. **Prove behavior**: differential oracle for every port; a watercup gate
   for pure helpers; both green under the central runner, not a self-test.
5. **Build and scan**, then commit. Commit per bite so a gateway crash cannot
   erase a wave.

## Phases

- **Phase 0 (done):** stop the in-flight adapter-remainder wave; it was more
  of the same dead credit.
- **Phase 1 (done):** `tests/check_dead_ports.py` is wired into
  `make workflow-check` (reality → dead-credit guard → build health). The
  guard fails on any NEW dead wired object against the tracked baseline of
  283 inherited dead files; the 264 wired ones await connect-or-strip triage.
- **Phase 2:** placeholder-cluster revival waves, one cluster per agent,
  with leaf oracles and the in-place-body rule.
- **Phase 3:** the binary front — get an image that links and runs under the
  SAC constraints; nothing counts as a finished port until it can execute.

No new adapter-remainder flow is dispatched until phases 1–2 have produced
verified, callable bodies.
