# Battleship v291 — DA-verified gaps

86 agent/tools modules PORTED at function-level.
8 real system-level gaps identified by DA.

## HIGH — Blocks UX parity

| # | Gap | C has | Python has | Fix |
|---|-----|-------|-----------|-----|
| 1 | **Web dashboard** | 0 | web_server.py (10K lines) | Wire api_server.c as gateway platform + serve dashboard |
| 2 | **Browser auth login** | Google OAuth only | Copilot, DingTalk, Dashboard auth flows | Add per-provider browser OAuth |
| 3 | **API server as gateway platform** | api_server.c standalone | api_server.py in gateway | Wire into gw_platform_t |
| 4 | **Gateway restart/lifecycle** | Basic server.c | GatewayRunner class | Add managed lifecycle |

## MEDIUM — Nice to have

| # | Gap | C depth | Python depth | Fix |
|---|------|---------|-------------|-----|
| 5 | Setup wizard | Working (~300L) | Full (6351L) | Add more provider auto-detect |
| 6 | Doctor | Basic | Full (2189L) | Add system diagnostics |
| 7 | Voice | Basic | Full (846L) | Add TTS pipeline |
| 8 | Plugin hooks | plugin_ext.c (286L) | 30+ plugin dirs | Add hook chain in agent loop |

## PORTED (function-level, DA-verified)

86 modules in agent/tools/gateway/CLI at ≥80% function parity.
Includes: all 30 CLI subcommands, 18 gateway platforms, 86 agent/tool modules.
986 PoP annotations across 156 files. Build: clean.
