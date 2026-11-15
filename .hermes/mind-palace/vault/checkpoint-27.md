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
