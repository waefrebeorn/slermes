<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Error Types — K01-K05

**Status: 5/5 (100%) — ✅ completed 2026-05-22**

- K01: ValueError — `hermes_error_t` with `HERMES_ERR_VALUE` type
- K02: TypeError — `HERMES_ERR_TYPE`
- K03: RuntimeError — `HERMES_ERR_RUNTIME`  
- K04: OSError — `HERMES_ERR_OS` with errno capture + strerror
- K05: TimeoutError — `HERMES_ERR_TIMEOUT`

**File:** `include/hermes_error.h`, `src/hermes_error.c`
**Tests:** `tests/test_hermes_error.c` — 11 tests
**API:** Macros `hermes_err_value()`, `hermes_err_type()`, `hermes_err_runtime()`, `hermes_err_os()`, `hermes_err_timeout()`, `hermes_errno()`. Thread-local `hermes_set_last_error()` / `hermes_get_last_error()`.
