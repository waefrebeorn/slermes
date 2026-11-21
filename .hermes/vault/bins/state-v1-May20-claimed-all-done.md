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

# Hermes C Translation — State (Archived v1, May 20)
**⚠️ OUTDATED — THIS DOCUMENT CONTAINS FALSE CLAIMS**
**Replaced by state.md (HONEST) on May 25.**
**See battleship.md for real gap breakdown.**

---

# Hermes C Translation — State ✅ ALL PHASES COMPLETE

## Phase 1: Foundation Dependencies ✅
| Dep | File | Status |
|-----|------|--------|
| JSON parser | `src/deps/json.c` | ✅ |
| YAML config parser | `src/deps/yaml.c` | ✅ |
| HTTP client (OpenSSL+sockets) | `src/deps/http.c` | ✅ |
| File-based session DB | `src/deps/db.c` | ✅ |
| Crypto (SHA-256/HMAC/base64) | `src/deps/crypto.c` | ✅ |
| Terminal display (ANSI) | `src/deps/cli_display.c` | ✅ |

## Phase 2: Agent Core ✅
| File | Status |
|------|--------|
| `agent_loop.c` | ✅ |
| `llm_client.c` | ✅ |
| `context.c` | ✅ |
| `title.c` | ✅ |
| `config.c` | ✅ |
| `cli.c` | ✅ |
| `display.c` | ✅ |
| `commands.c` | ✅ |
| `main.c` | ✅ |

## Phase 3: Tools ✅
| File | Status |
|------|--------|
| `registry.c` | ✅ |
| `terminal.c` | ✅ |
| `file.c` | ✅ |
| `web.c` | ✅ |
| `skills.c` | ✅ |
| `tool_init.c` | ✅ |

## Phase 4: Gateway ✅
| File | Status |
|------|--------|
| `server.c` | ✅ |
| `platforms/telegram.c` | ✅ |

## Phase 5: Cron + Advanced ✅
| File | Status |
|------|--------|
| `scheduler.c` | ✅ |
| `jobs.c` | ✅ |

## Build
- binary: `hermes` — 342KB, all 5 phases
- deps: OpenSSL ✓ (dynamic), no other system libs
- `make` → clean build, zero errors
- usage: `./hermes` (CLI), `./hermes gateway` (Telegram), `./hermes cron`, `./hermes --version`
