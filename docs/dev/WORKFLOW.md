# Gap-closing workflow — Hermes orchestration contract

Decision record, 2026-11-21. Extends REORGANIZATION.md (the *what*) with the
*how*: who does the work, in what order, with which proofs, and how the
organization survives crashes, SAC, and this box's MSYS fragility.

## The queue is the single source of truth

`docs/dev/gap-queue.jsonl`, owned by `scripts/gap_queue.py`:

```bash
python scripts/gap_queue.py --regen        # re-derive from LIVE parity_reality
python scripts/gap_queue.py --top 10       # next queued bites, ranked
python scripts/gap_queue.py --claim ID --owner NAME
python scripts/gap_queue.py --verify ID --evidence .testbin/x.log
python scripts/gap_queue.py --land ID --commit HASH --evidence .testbin/x.log
```

- `ph:<c_file>` entries are placeholder clusters ranked by size; the feature
  list (py module, py fn, c name) is regenerated from reality, never typed.
- `dbg:<name>` entries are open oracle-debug targets (docs/dev/debug-queue.md).
- Status flow: `queued -> claimed -> verified -> landed`. `--regen` preserves
  status/owner/evidence/commit; a claimed cluster whose placeholders vanished
  becomes `gone-from-reality` and must be re-verified, never auto-credited.
- No hand-tallied progress numbers anywhere. The queue + the live tools are
  the scoreboard; commit messages carry the oracle evidence.

## Two lanes, one tree

**Authoring lane (parallel, `delegate_task`, max 3 children).** One child per
queue entry. Children re-port bodies, write harness/oracle/fixtures, and run
*leaf* oracles only. Cap of 3 is a hard limit: heavy concurrent MSYS load
fork-poisons the shell registry ("No shell found" everywhere), and only one
full build may run at a time.

**Gate/landing lane (serial, the orchestrating main session).** The parent
alone runs whole-project gates, commits, and pushes. Child summaries are
self-reports, not facts: the parent re-runs every oracle itself from the
`.testbin` log before landing a bite.

Never point the Kanban dispatcher at this tree while the main session is
orchestrating: one profile (`default`) exists, workers would serialize anyway,
and a second writer in the same working tree corrupts bites. The git-tracked
queue is the durable backlog; the board is at most an audit mirror.

## Child contract (paste into every dispatch)

1. Load skills `slermes-windows-port` and `slermes-windows-build` first.
2. Repo `C:\Users\eman5\slermes`; quarry `C:\Users\eman5\hermes-agent` is
   READ-ONLY ground truth. Work ONLY your assigned queue entry's C file(s)
   plus their tests/harness/oracle/fixtures.
3. Every compile/link goes through one MSYS2 child:
   `/c/msys64/usr/bin/bash -lc 'source /c/Users/eman5/slermes/build/winlibs-env.sh; cd /c/Users/eman5/slermes; ...'`
4. ALLOWED: `make <specific .o>` (rebuild your own objects — the runner never
   does), `bash tests/run_one_oracle.sh <name> <oracle.py> "" "<objs>"`,
   leaf gcc harness compiles, nm, read-only scans.
   FORBIDDEN: `make slermes` / `make all`, verify_build_health,
   run_all_oracles, slv suites, watercup fleet, `git commit` / `git push`,
   new `*_flow*.c` files, edits outside your entry.
5. In-place rule: replace the placeholder body inside the assigned file; keep
   the PoP claim honest; the symbol must be exported (`nm -g --defined-only`
   shows `T`) and its object wired in `build/objects.mk`. A static-only port
   is not done.
6. Proof: differential oracle against LIVE Python (ast-extracted, scripted
   seams, emit inputs from C). Final line must be exactly
   `ORACLE <name>: N cases, 0 mismatches` (the word `cases`, never `rows`).
   Log to `.testbin/<name>.log`; confirm GATE rc=0 and harness exit 0.
7. Report back: files touched, the ORACLE line, log path, nm lines, and what
   stayed unported and why. Blocked (SAC rc 126/127)? Retry with backoff,
   then report the blocker honestly — never fabricate a result.

## Parent landing procedure (per bite)

1. Re-run the child's oracle yourself; read the `.testbin` log; require
   `N cases, 0 mismatches`, rc=0.
2. `nm -g --defined-only` the touched objects: re-ported symbols exported.
3. `python tests/verify_build_health.py --no-link` (MSYS2 child) — N/N clean,
   port-object gate PASS.
4. Commit per bite with the WaefreBeorn identity (`-c` flags BEFORE the
   subcommand), message naming cluster + function count + oracle result.
   Verify `git show --stat HEAD` touched exactly the intended set.
5. `python scripts/gap_queue.py --land <id> --commit <hash> --evidence <log>`;
   push to `origin main` (portable-git procedure, MAIN-ONLY).

## Sweeps (background, serialized)

Per-bite gates are leaf oracle + build health. The full standing gates —
`run_all_oracles.sh` (ORACLE_JOBS=1), `run_slv_tests.sh`, `make parity` — run
as periodic sweeps after landed waves, never concurrently with a full build.
A red sweep is the next queue entry, not a footnote. `make parity` refreshes
every sentinel; never hand-edit a sentinel number.

## Crash resilience

- Commit per verified bite: a gateway crash can never erase a wave. (The
  2026-11-21 reorientation rescued 168 green oracle cases of uncommitted
  in-flight work — that must not recur.)
- On resume: `git log --oneline -5`, `git status --short`,
  `python scripts/gap_queue.py --regen`, read `claimed` entries and their
  `.testbin` logs, re-verify or finish them before dispatching new work.
- Stale `claimed` entries with no live child are re-queued, not trusted.

## Scale-up path (not active)

If a hands-off fleet is wanted later: dedicated worker profiles + one git
worktree per worker (isolated trees, serialized link lane via lockfile),
Kanban cards assigned to those profiles with `goal_mode=True`. Until then,
delegate_task fan-out + parent-verified landing is the organization.