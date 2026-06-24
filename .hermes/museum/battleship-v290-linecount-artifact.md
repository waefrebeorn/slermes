# Battleship v290 — REAL Gaps (System-Level)

**The old "86 PORTED" was function-level. These are SYSTEM-LEVEL gaps.**
**Old battleship archived to `.hermes/museum/`.**

## Tier 1 — Massive Gaps (<30% parity)

| # | Subsystem | Python (lines) | C (lines) | Parity | Priority |
|---|-----------|---------------|-----------|--------|----------|
| 1 | **CLI/Setup** | 140,467 | 23,967 | 17% | HIGH |
| 2 | **Gateway** | 80,057 | 19,098 | 24% | HIGH |
| 3 | **TUI Gateway bridge** | 9,916 | 0 | 0% | MED |
| 4 | **Web dashboard** | 10,080 | 0 | 0% | MED |
| 5 | **Desktop client** | EXISTS | 0 | 0% | LOW |

## Tier 2 — Significant Gaps (30-60% parity)

| # | Subsystem | Parity | Key missing |
|---|-----------|--------|-------------|
| 6 | **CLI Auth** | 17% | OAuth PKCE, Copilot auth, Dashboard auth, DingTalk auth |
| 7 | **CLI Web server** | 0% | Dashboard, API server not wired as gateway platform |
| 8 | **CLI Doctor/Diagnostics** | 0% | `hermes doctor`, `hermes debug` |
| 9 | **CLI Backup** | 0% | `hermes backup`, config migration |
| 10 | **Gateway sessions** | 24% | Session persistence, context bridging |
| 11 | **Gateway platforms** | 24% | 5 missing: google_chat, irc, line, ntfy, simplex |
| 12 | **ACP tools** | 46% | Edit approval, events, permissions, resources |

## Tier 3 — Edges (60-80% parity)

| # | Subsystem | Parity | Key missing |
|---|-----------|--------|-------------|
| 13 | **Agent** | 66% | Credits lifecycle, plugin hooks, scrubbers |
| 14 | **Tools** | 54% | Skill management commands |
| 15 | **Cron** | 64% | Cron CLI commands |

## N/A — Correctly Classified

| Subsystem | Reason |
|-----------|--------|
| Python plugins (>30 dirs) | SDK-level Python, not portable |
| Skills content | Markdown/yaml data, not code |
| Tests (100k+ lines) | Python unittest/pytest, not portable |
| Website/docs | Docusaurus, different technology |
| CI/CD pipelines | GitHub Actions, not portable |

## Build
Clean.
