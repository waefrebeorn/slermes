# State — Slermes C Translation (v540)

- Build: `make -j$(nproc)` = 0 errors
- **Scanner (real, this session):** 4,664 PORTED (47.9%), 5,067 REAL_GAP (52.1%), 9,731 total features
- Tests: `bash tests/run_mission8_tests.sh` → 36 passed, 0 failed, 35 skipped
- Façade audit (edict: no fake-looking code): 57 banned "In a real implementation" stubs across 19 files in src/cli/; 1 file fully closed (port_tools_url_safety.c), 18 files + 54 stubs remaining
- Desktop parity: 111 features mapped, ~99 missing (4% complete)
- Prior walkway claims of "8,688/8,688 100% PORTED" are stale/v398-era fiction; corrected to live scanner output above

## This Session (v540)
- Rewrote src/cli/port_tools_url_safety.c façades into REAL implementations:
  - normalize_url_for_request: real RFC 3492 punycode (IDNA) per label + percent-encoding, verified byte-for-byte vs Python (Köln→K%C3%B6ln, Bücher.de→xn--Bcher-kva.de, münchen.de→xn--mnchen-3ya.de, port + userinfo preserved)
  - _global_allow_private_urls: now reads security.allow_private_urls + browser.allow_private_urls from config.yaml (YAML line-scan) + HERMES_ALLOW_PRIVATE_URLS env
  - async_is_safe_url: honest comment (no event loop in C)
- Removed god-header hermes.h from the file; removed dead CGNAT consts
- Fixed 4 punycode bugs (extra +BASE, digit char map, dropped bias update, strtok_r buffer corruption)
- Confirmed _is_blocked_ip already fully implements SSRF blocks (incl. CGNAT 100.64/10)
- Saved skill slermes/url-safety-c-port with verified encoder + build/test recipe
- Build CLEAN, 36/36 tests pass
- NOT YET COMMITTED (dirty tree carries prior-session uncommitted work too)
