# Roadmap — Slermes C Translation (v670)

## Current State

> Live counts are owned by `make parity-walkway` — the table below is the
> authoritative sentinel (do not hand-edit).

<!-- PARITY:AUTO -->
| PORTED  | 13,022 / 14,045 (92.7%) |
| REAL_GAP| 1,020 (7.3%) — no N/A |
| PARTIAL | 3 (0.0%) |
| BOOTLEG | 0 (recursive_false_gap_hunter.py) |

**Phase (v670):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,294 ahead / 495 behind upstream/main (last merge 2026-08-07 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-08-07T21:21:17Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->

**Rewriting from scratch in C is the point.** Stubs that just log+return NULL are REAL_GAP.

## The REAL_GAPs — Implementation Roadmap

### M1: CLI Core (2,632 gaps, 161 modules) — HIGHEST PRIORITY
User-facing CLI commands. Start here.

| Module | Gaps | Total | Port File |
|--------|------|-------|-----------|
| hermes_cli/web_server.py | 303 | 309 | src/cli/hermes_cli_web_server.c |
| cli.py | 246 | 268 | src/cli/port_cli.c |
| hermes_cli/main.py | 183 | 213 | src/cli/hermes_cli_main.c |
| hermes_cli/auth.py | 182 | 190 | src/cli/hermes_cli_auth.c |
| hermes_cli/kanban_db.py | 154 | 158 | src/cli/hermes_cli_kanban_db.c |
| hermes_cli/gateway.py | 141 | 155 | src/cli/hermes_cli_gateway.c |
| hermes_cli/models.py | 82 | 91 | src/cli/hermes_cli_models.c |
| hermes_cli/config.py | 67 | 76 | src/cli/hermes_cli_config.c |
| hermes_cli/tools_config.py | 59 | 60 | src/cli/hermes_cli_tools_config.c |
| hermes_cli/kanban.py | 54 | 58 | src/cli/hermes_cli_kanban.c |
| hermes_cli/plugins.py | 54 | 61 | src/cli/hermes_cli_plugins.c |
| ... 151 more | ... | ... | ... |

### M2: Gateway Platforms (1,854 gaps, 56 modules) — HIGH PRIORITY
Platform adapters (Telegram, Discord, Slack, Feishu, etc.)

| Module | Gaps | Total | Port File |
|--------|------|-------|-----------|
| gateway/run.py | 225 | 239 | src/cli/port_gateway_run.c |
| gateway/platforms/feishu.py | 195 | 208 | src/cli/port_gateway_platforms_feishu.c |
| gateway/platforms/yuanbao.py | 170 | 210 | src/cli/port_gateway_platforms_yuanbao.c |
| gateway/platforms/telegram.py | 132 | 145 | src/cli/port_gateway_platforms_telegram.c |
| gateway/platforms/matrix.py | 104 | 120 | src/cli/port_gateway_platforms_matrix.c |
| gateway/platforms/base.py | 103 | 155 | src/cli/port_gateway_platforms_base.c |
| gateway/platforms/qqbot/adapter.py | 77 | 89 | src/cli/port_gateway_platforms_qqbot_adapter.c |
| gateway/platforms/weixin.py | 76 | 102 | src/cli/port_gateway_platforms_weixin.c |
| gateway/platforms/slack.py | 58 | 69 | src/cli/port_gateway_platforms_slack.c |
| gateway/platforms/wecom.py | 55 | 67 | src/cli/port_gateway_platforms_wecom.c |
| ... 46 more | ... | ... | ... |

### M3: Tools (1,491 gaps, 95 modules) — MEDIUM PRIORITY
Tool implementations (browser, file, terminal, MCP, etc.)

| Module | Gaps | Total | Port File |
|--------|------|-------|-----------|
| tools/skills_hub.py | 153 | 181 | src/cli/port_tools_skills_hub.c |
| tools/mcp_tool.py | 76 | 84 | src/cli/port_tools_mcp_tool.c |
| tools/browser_tool.py | 64 | 75 | src/cli/port_tools_browser_tool.c |
| tools/tts_tool.py | 64 | 68 | src/cli/port_tools_tts_tool.c |
| tools/file_operations.py | 49 | 66 | src/cli/port_tools_file_operations.c |
| tools/approval.py | 32 | 47 | src/cli/port_tools_approval.c |
| ... 89 more | ... | ... | ... |

### M4: Agent (363 gaps, 59 modules) — MEDIUM PRIORITY
Agent runtime, providers, credentials

| Module | Gaps | Total | Port File |
|--------|------|-------|-----------|
| agent/auxiliary_client.py | 67 | 132 | src/cli/port_agent_auxiliary_client.c |
| agent/memory_manager.py | 31 | 42 | src/cli/port_agent_memory_manager.c |
| agent/credential_pool.py | 16 | 57 | src/cli/port_agent_credential_pool.c |
| agent/coding_context.py | 14 | 25 | src/cli/port_agent_coding_context.c |
| ... 55 more | ... | ... | ... |

### M5: Cron (86 gaps, 6 modules) — LOW PRIORITY
Cron job management

| Module | Gaps | Total | Port File |
|--------|------|-------|-----------|
| cron/jobs.py | 33 | 34 | src/cli/port_cron_jobs.c |
| cron/scheduler.py | 27 | 30 | src/cli/port_cron_scheduler.c |
| cron/suggestions.py | 11 | 12 | src/cli/port_cron_suggestions.c |
| cron/blueprint_catalog.py | 8 | 9 | src/cli/port_cron_blueprint_catalog.c |
| cron/scripts/classify_items.py | 5 | 6 | src/cli/port_cron_scripts_classify_items.c |
| cron/suggestion_catalog.py | 2 | 2 | src/cli/port_cron_suggestion_catalog.c |

## Implementation Strategy

### Per-Function Process
1. Read the Python source function
2. Understand what it does
3. Replace the stub with real C implementation:
```c
// OLD (REAL_GAP):
void* func(void* p1, ...) {
    hermes_log(LOG_DEBUG, "port", "func called");
    return NULL;
}

// NEW:
void* func(void* p1, ...) {
    // Actual logic matching Python behavior
    // ...
    return result;
}
```
4. Build, test, commit
5. Scanner re-run to verify PORTED++ REAL_GAP--

### Priority Order
1. Lowest-gap-count modules first (quick wins → momentum)
2. Then highest-gap-count modules (biggest impact)
3. Within each module: helpers first, then callers
