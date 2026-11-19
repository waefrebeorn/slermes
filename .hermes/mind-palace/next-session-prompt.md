<!-- PARITY:AUTO -->| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Next Session Prompt — copy-paste ready

```
Goal: Close function-level parity gaps (REAL_GAP) across ALL Python modules
and SLERM the third-party dependencies. Rewriting from scratch in C is the
point; there is NO N/A and nothing "not doable".

Form-not-function gap: any C function whose body is (a) a hardcoded return
default with an excuse comment, (b) printf+return-0 echo, or (c) a PoP
annotation with an empty/stub body — is a REAL_GAP. Close it with real logic.

Live counts: run `make parity` and read the sentinel. Do NOT trust any
hand-typed number in this prompt — the sentinel is the only live truth.

System:
- make parity        -> scan + watercup + walkway (refresh all sentinels)
- bash tests/run_all_oracles.sh   -> differential oracle gate (ORACLE_JOBS=1)
- bash tests/run_slv_tests.sh     -> slerm-Lua suites
- python tests/verify_build_health.py --no-link  -> build + port-object gate
- python tests/parity_reality.py  -> body-level honest scoreboard

Priority:
1. Close pure-function REAL_GAPs (oracle + watercup proof per bite).
2. Expand watercup coverage; fix ported-gate harness signatures.
3. SLERM the dependency front (c11_whisper, SQLite engine, ncurses->Lua).
4. Sync upstream when the sentinel's behind-count is non-zero.

Loop: scanner detail -> grep src/include/lib before writing -> implement or
alias -> oracle + watercup -> build -> commit to integrate. One module at a
time; never stop between gaps. Behavior first, annotations last.
```
