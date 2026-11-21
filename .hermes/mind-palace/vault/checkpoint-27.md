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

# Checkpoint 27 — EN08 Docker file_sync, EN08 now fully wired

## Gaps closed

### EN08 (file_sync) — PARTIAL → more complete
- `src/tools/terminal.c:434-446` — Added ~/.hermes volume mount to Docker backend
- Docker backend now mounts host's ~/.hermes into container at the same path (read-only)
- Combined with checkpoint 25's SSH scp-based upload, file_sync is now wired for both SSH and Docker backends
- Still missing: Modal backend (requires Modal SDK), sync-back on cleanup

## Build
- Clean compile, 0 errors
- 4/4 tests pass
