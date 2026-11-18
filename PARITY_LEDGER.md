# Slermes ↔ Hermes Parity Ledger (honest, whole-project)

**This file is a dated snapshot, not a live source.** The live numbers are
owned by `make parity` and stamped into the `<!-- PARITY:AUTO -->` sentinels
(README, BANNER, ROADMAP, docs/parity-summary.md, .hermes/). When this ledger
and a sentinel disagree, the sentinel is newer — regenerate this snapshot
from the commands below rather than trusting either from memory.

Snapshot measured **2026-11-18** on the Windows dev box, branch `integrate`
(latest `make parity` run; the sentinels carry the live copy).

## The three audits (different denominators, on purpose)

| Audit | Command | Question it answers |
|---|---|---|
| Battleground scanner | `python scripts/parity_truth.py` | Which Python functions carry a verifiable C PoP claim? (static; a stub with a comment still counts) |
| Body-level reality | `python tests/parity_reality.py` | Which C bodies actually do work? (`(void)x; return 0;` = placeholder, not implemented) |
| Port stages | `python scripts/gen_port_stages.py` | Which modules have advanced unit → header → harness → oracle → green → MATCH? |

The scanner is triage; reality is the honest scoreboard; stages is the
funnel that answers "what next". A port is parity-closed only when its
differential oracle **and** its watercup gate say MATCH — no static audit
can grant that.

## Scanner snapshot (feature-level, PoP-claimed)

`scripts/parity_truth.py` → `live_parity_scan.json`, fail-closed (refuses to
emit unless the Python quarry is present and actually scanned):

- Total features: **34,262** across **1,978** modules
- **PORTED: 8,377 (24.4%)**
- PARTIAL: 45 · STUB: 0 · BOOTLEG: 2 (hunter)
- **REAL_GAP: 25,840** — of which DA-4 annotation-only stubs: **13**,
  genuine gaps (no C at all): **25,827**
- Annotations: 26,686 total · 20,521 claimed · 19,875 verifiable ·
  **611 orphan** (claimed module absent from both trees and no twin) ·
  6,165 unattributed (loose prose, no module path) · 61 non-python claims

Orphan ≠ unattributed: an orphan is a claim that cannot be checked;
unattributed is a comment that makes no claim. Never fold them together.

### By area (scanner scope: the five production packages)

| Area | Features | Ported | % | Real gaps |
|---|---:|---:|---:|---:|
| hermes_cli | 14,049 | 3,431 | 24.4% | 10,594 |
| agent | 7,079 | 1,835 | 25.9% | 5,243 |
| tools | 6,213 | 1,866 | 30.0% | 4,328 |
| gateway | 6,135 | 971 | 15.8% | 5,158 |
| cron | 786 | 252 | 32.1% | 534 |

(`python scripts/parity_coverage.py --dirs` rolls up a wider module set —
plugins, tui_gateway, pm, acp_adapter, hermes_platform — at module level;
its percentages are a different cut, not a contradiction.)

## Body-level reality snapshot

`tests/parity_reality.py` (placeholder bodies do not count):

- Total Python features extracted: **35,858**
- **Implemented: 12,234 (34.1%)**
- Placeholder only: **704 (2.0%)**
- Missing entirely: **22,920 (63.9%)**

The reality audit extracts more features than the scanner and judges bodies,
not comments — which is why its totals differ. Quote the audit that matches
the question.

## Method notes

- **PORTED** (scanner): a Python function whose C port carries a
  `/* PoP: <cfn> @ <module.py>:<pyfn> */` annotation naming that module+fn,
  and the C unit is linked (the scanner indexes only files named in
  `build/objects.mk` — parity is a claim about the binary).
- **REAL_GAP**: no verifiable C counterpart. There is no N/A: rewriting from
  scratch in C is the point, so every unported feature is work.
- The DA-4 bucket is the cheat surface: PoP comment, no real body. It is
  reported separately so it can never hide inside PORTED.
- `tests/recursive_false_gap_hunter.py` hunts BOOTLEG echo stubs; the
  pipeline runs it and stamps the count (0 at this snapshot).

## Regenerate

```bash
make parity        # scan -> watercup -> walkway: refreshes every sentinel
python tests/parity_reality.py
python scripts/gen_port_stages.py
```

Never hand-edit a number inside a sentinel, and never quote a snapshot as
live. A number copied by hand is a number that has already gone stale.
