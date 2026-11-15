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

# Checkpoint 53 — AG25 extract_rate_limit_reset ported

**Battleship:** v79→v80

## What was done

### AG25: extract_api_error_context → extract_rate_limit_reset

- New function `extract_rate_limit_reset()` added to `src/agent/llm_client.c:1543-1624`
- Port of Python `agent/agent_runtime_helpers.py:extract_api_error_context()`
- Extracts rate-limit reset timing from HTTP response headers:
  - `Retry-After` header (RFC 7231) — seconds or HTTP-date, parsed via `http_parse_retry_after()`
  - `X-RateLimit-Reset` header — Unix timestamp or seconds-from-now
- Falls back to JSON body fields: `retry_after` (seconds from now), `reset_at` / `resets_at` (Unix timestamps)
- Wired into `credential_pool_report()` call at `llm_client.c:1037` — replaces hardcoded `0` with actual extracted value
- Previously credential_pool_report defaulted to 60s delay on 429s; now respects actual server timing

### Build/Test
- Clean compile, 0 errors
- 4/4 tests passing

### Classification Changes
- AG25 agent_runtime_helpers: 5/24 → 6/24 functions ported (21% → 25%)