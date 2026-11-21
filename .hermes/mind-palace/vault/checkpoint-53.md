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