# Plan — Slermes C Translation (v570)

## Current State
- 8,688/8,688 PORTED (100%) — Python→C port complete
- Build: CLEAN, scanner: 100%
- Triple DA audit suite: created

## Next: Desktop/Web Parity (v506-v470)
1. **Terminal/PTY** — libvterm + posix_openpt/CreatePseudoConsole
2. **Chat rendering** — markdown/code rendering in C
3. **WebSocket client** — libwebsockets for gateway connection
4. **Native OS integrations** — clipboard, notifications, file dialogs
5. **Web app API** — expand web_app.c from 7 to 30+ endpoint families
6. **CUA replacement** — native C11 desktop framework

## Priority Order
1. P0 desktop features (terminal, PTY, chat, clipboard, file ops)
2. P0 web app features (API endpoints)
3. P1 desktop features (profiles, models, settings, sessions)
4. P1 web app features (auth, config, updates)
5. P2 desktop features (marketplace, haptics, translucency)

<!-- PARITY:AUTO -->
| PORTED  | 5,385 / 9,731 (55.3%) |
| REAL_GAP| 4,270 (43.9%) — no N/A |
| PARTIAL | 76 (0.8%) |
| STUB    | 0 |

_Generated from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
