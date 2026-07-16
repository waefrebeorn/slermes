# Next Session Prompt — Copy-Paste Ready

---

```
Goal: Close function-level parity gaps (REAL_GAP) across ALL Python modules —
and continue feature/API parity for desktop, web, skills, docs, scripts, tests,
configs. Rewriting from scratch in C is the point; there is NO N/A.

State (live scanner 2026-07-12):
- PORTED: 4,924 / 9,731 (50.6%)
- REAL_GAP: 4,732 (48.6%)  ← all real work, no exemptions
- PARTIAL: 75 (0.8%)  STUB: 0  N/A: 0
- Build: clean, single native binary (~40MB)
- Tests: mission8 36/0 green; oracles 0 mismatch on ported modules

Where the gaps are (by subsystem, 318 modules with ≥1 gap, 76 never-started):
- hermes_cli/: 2,401 gaps across 170 modules
- gateway/:     1,109 gaps across 36 modules
- tools/:         732 gaps across 56 modules
- agent/:         368 gaps across 49 modules
- cron/:          102 gaps across 6 modules

The Loop (no stops, no "time" excuses — agentic work takes minutes):
1. Pick next REAL_GAP (Tier 1 tail modules first: ≥50% ported, big absolute
   gap → fastest to 0 REAL_GAP). Read the Python source, implement real C.
2. Add /* PoP: c_func @ module.py:py_func */ before the C function.
3. Register the .o in build/objects.mk (append var into PHASE5_OBJ; do NOT use
   PHASE*_OBJ += — dead).
4. make slermes clean + build; oracle-verify (tests/oracle/runners/run_oracle.sh).
5. Commit + push. Pick next gap. Never pause between gaps.

Highest-ROI pure-logic targets (oracle-able): tools/ (cua_backend 43,
delegate_tool 37, file_tools 35, skills 32, terminal_tool 31, patch 28,
transcription 27, voice_mode 26, skills_hub 25, computer_use 24, project 22,
approval 21, browser_tool 21, shell 20, threat 20).

Tier 2 tail modules (finish to 0): cli.py 94% (20 gaps), hermes_cli/config.py
73% (25), browser_tool.py 66% (34), gateway/platforms/base.py 52% (79),
agent/account_usage.py 94% (1), etc.

There is no "defer"/"out of scope"/"too big". Big modules get finished.
```

<!-- PARITY:AUTO -->
| PORTED  | 5,570 / 9,731 (57.2%) |
| REAL_GAP| 4,064 (41.8%) — no N/A |
| PARTIAL | 97 (0.8%) |
| STUB    | 0 |

_Generated from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
