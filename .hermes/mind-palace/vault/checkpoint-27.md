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
