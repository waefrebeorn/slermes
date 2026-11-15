<!-- PARITY:AUTO -->
| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

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
