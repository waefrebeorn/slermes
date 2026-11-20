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
