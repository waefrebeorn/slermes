# Next Session: Function + Feature Parity — Steamroll All Remaining REAL_GAPs

## Goal: Close the 4,732 REAL_GAPs. Rewriting from scratch in C is the point;
there is NO N/A and no "out of scope". Agentic work takes minutes.

## State at Start (live, 2026-07-12)
- Version: v572-era (NA machinery deleted; honesty + no-time-excuse sweep done)
- Build: Clean, 0 errors (single native `slermes` binary, ~40MB)
- Function parity: 4,924 / 9,731 PORTED (50.6%) — NOT complete
- REAL_GAP: 4,732 (48.6%)  PARTIAL: 75  STUB: 0  N/A: 0
- Tests: mission8 36/0 green; oracles 0 mismatch on ported modules

## Gap distribution (318 modules with ≥1 gap; 76 never-started)
- hermes_cli/: 2,401 gaps / 170 modules
- gateway/:    1,109 gaps / 36 modules
- tools/:        732 gaps / 56 modules
- agent/:        368 gaps / 49 modules
- cron/:         102 gaps / 6 modules

## Missions in order (steamroll, no stops)
1. Tier 1 tail modules → 0 REAL_GAP (fastest wins):
   cli.py (94%, 20), hermes_cli/config.py (73%, 25), browser_tool.py (66%, 34),
   gateway/platforms/base.py (52%, 79), agent/account_usage.py (94%, 1), …
2. Pure-logic tools/ (oracle-able, highest quality):
   cua_backend 43, delegate_tool 37, file_tools 35, skills 32, terminal_tool 31,
   patch 28, transcription 27, voice_mode 26, skills_hub 25, computer_use 24, …
3. Big under-ported hubs:
   web_server.py 335, gateway/run.py 272, main.py 176, auth.py 150, pet/store 142
4. 76 never-started modules (all REAL_GAP work):
   gateway/slash_commands.py 56, hermes_cli/cli_commands_mixin.py 43,
   agent/pet/generate/atlas.py 33, tools/read_extract.py 31, …

## Execution Rules
1. Pick next REAL_GAP. Read Python source; implement real C (no stubs, no
   void* passthrough, no "not implemented in C" strings).
2. Add /* PoP: c_func @ module.py:py_func */ immediately before the C function.
3. Register .o in build/objects.mk — append into PHASE5_OBJ (NOT PHASE*_OBJ +=).
4. `rm -f slermes <touched>.o && make slermes`; oracle-verify via
   `bash tests/oracle/runners/run_oracle.sh <name>` → 0 mismatches.
5. Commit after each module; push. Update walkway state.md to live numbers.
6. No "defer"/"for now"/"out of scope"/"too big" — finish the module.

## Key Project Info
- Root: /home/wubu/hermes-agent-dev/slermes/   Branch: main
- Scanner: python3 tests/slermes_parity_battleground.py --json
- Oracle harness: tests/oracle/ (fixtures + run_oracle.sh)
- Pre-commit workaround: PRE_COMMIT_ALLOW_NO_CONFIG=1 git commit …

## What "Done" Looks Like
- Every Python function has a real C implementation with a PoP annotation.
- Scanner reads 0 REAL_GAP, 0 PARTIAL, 0 STUB (N/A does not exist).
- Build clean; oracles 0 mismatch; committed + pushed.

<!-- PARITY:AUTO -->
| PORTED  | 5,385 / 9,731 (55.3%) |
| REAL_GAP| 4,270 (43.9%) — no N/A |
| PARTIAL | 76 (0.8%) |
| STUB    | 0 |

_Generated from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
