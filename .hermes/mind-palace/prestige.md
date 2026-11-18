<!-- PARITY:AUTO -->| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Prestige — session wrap-up ritual

Prestige is earned by verified bites, not by narrative. At session end:

1. **Gates green** — build health, oracle suite, slv suites, `make parity`
   all run and passing (or every failure named with its cause). A gate that
   did not run is not a pass.
2. **Commits landed** — each bite self-contained on `integrate`, message
   stating what is proven (cases, mismatches, gate results). Source only,
   no artifacts.
3. **Sentinels refreshed** — `make parity` stamped every walkway file from
   live sources; no hand-typed numbers left behind.
4. **Handoff written** — `next-session-prompt.md` updated with the next
   concrete bite, derived from the live gap list.
5. **History preserved** — superseded session artifacts moved to
   `archive/` / `../museum/`, never deleted, never quoted as current.

The ritual's only metric is the honest one: real bodies doing real work,
proven against LIVE Python. A falling gap count with stubs behind it is not
prestige — it is debt.
