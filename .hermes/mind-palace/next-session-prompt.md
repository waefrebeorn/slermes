# Next Session Prompt — Copy-Paste Ready (v672)

---

```
Goal: Close function-level parity gaps (REAL_GAP) across ALL Python modules.
Rewriting from scratch in C is the point; there is NO N/A.

Form-not-function gap: any C function whose body is (a) a hardcoded return
default with an excuse comment, (b) printf+return-0 echo, or (c) a PoP annotation
with an empty/stub body — is a REAL_GAP. Vault + close, do not hand-wave.

Live counts:
| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |

Watercup oracle: 313 PASS / 22,604 total (1.4%). When a gap closes, its watercup
auto-flips MISMATCH → MATCH.

System:
- `make parity` → refresh all walkway files from live scanner
- `make watercup` → run 22,604 trace-cup gates in parallel (~6 sec)
- `make test` → Mission 8: 77 tests
- `make build` → Clean, 0 errors

Priority:
1. Fix ported watercup gates (read real C signatures, emit correct harnesses)
2. Close pure-function gaps (no I/O, no state, no complex types)
3. Sync upstream (2,639 behind)

Upstream: 2,072 ahead / 2,639 behind (last merge 2026-09-14).
```
