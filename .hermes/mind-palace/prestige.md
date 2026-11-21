<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
