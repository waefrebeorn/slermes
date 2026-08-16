#!/usr/bin/env python3
"""Build the explicit merge-resolution table for the 43 non-mechanical cases.

Groups every collision + real-clobber into a "Python module -> set of C files"
cluster, and picks a CANONICAL (the file that keeps its name and absorbs the
others). Rule: canonical = the file whose name already matches the module's
1:1 target (clean name, correct folder); if several, the larger (more lines).
All other members get their unique functions appended into canonical, then
deleted. Prints the plan; does NOT mutate.
"""
import os
import re

ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
MARKER = re.compile(r'Port of Python\s+([a-z_]+)/([A-Za-z0-9_]+)\.py', re.I)
PKG_FIX = {'hermes_cli': 'hermes_cli'}

# Seed clusters from the 39 collisions + 4 clobbers found by validate2
SEED = [
 ('src/agent/config_py_final.c','src/agent/port_gateway_config_py_final.c','src/agent/port_hermes_cli_config_py_final.c'),
 ('src/agent/antigravity_oauth.c','src/agent/port_antigravity_oauth.c','src/agent/port_agent_antigravity_oauth.c'),
 ('src/agent/health.c','src/agent/port_cron_health.c','src/agent/port_gateway_health.c'),
 ('src/agent/auxiliary_client.c','src/agent/port_auxiliary_client.c','src/agent/port_agent_auxiliary_client.c'),
 ('src/agent/auxiliary_client_gaps.c','src/agent/port_auxiliary_client_gaps.c','src/agent/port_agent_auxiliary_client_gaps.c'),
 ('src/cli/port_voice.c','src/hermes_cli/voice.c','src/cli/port_hermes_cli_voice.c'),
 ('src/cli/port_config_helpers.c','src/hermes_cli/config_helpers.c','src/cli/port_tools_config_helpers.c'),
 ('src/cli/port_main.c','src/hermes_cli/main.c','src/cli/main.c'),
 ('src/cli/port_hermes_cli_hooks.c','src/hermes_cli/hooks.c','src/cli/port_gateway_hooks.c'),
 ('src/cli/port_anthropic_adapter.c','src/hermes_cli/anthropic_adapter.c','src/cli/port_agent_anthropic_adapter.c'),
 ('src/cli/port_doctor.c','src/hermes_cli/doctor.c','src/cli/doctor.c'),
 ('src/cli/port_config.c','src/hermes_cli/config.c','src/cli/port_gateway_config.c'),
 ('src/cli/cli.c','src/hermes_cli/cli.c','src/cli/port_cli.c'),
 ('src/cli/port_agent_display.c','src/hermes_cli/display.c','src/cli/display.c'),
 ('src/hermes_cli/port_agent_plugins_gaps.c','src/hermes_cli/plugins_gaps.c','src/hermes_cli/port_plugins_gaps.c'),
 ('src/hermes_cli/port_service_manager_gaps.c','src/hermes_cli/service_manager_gaps.c','src/hermes_cli/port_hermes_cli_service_manager_gaps.c'),
 ('src/hermes_cli/port_batch3_gaps.c','src/hermes_cli/batch3_gaps.c','src/cli/port_cli_batch3_gaps.c'),
 ('src/gateway/port_platforms_base.c','src/gateway/platforms_base.c','src/gateway/port_gateway_platforms_base.c'),
 ('src/gateway/port_gateway_run_gaps.c','src/gateway/run_gaps.c','src/gateway/port_run_gaps.c'),
 ('src/cron/port_cron_jobs.c','src/cron/jobs.c','src/cron/port_jobs.c'),
 ('src/ports/agent/turn_context.c','src/agent/turn_context.c','src/agent/port_agent_turn_context.c'),
 ('src/ports/agent/markdown_tables.c','src/agent/markdown_tables.c','src/agent/port_markdown_tables.c'),
 ('src/ports/agent/title_generator.c','src/agent/title_generator.c','src/agent/port_agent_title_generator.c'),
 ('src/ports/agent/prompt_builder.c','src/agent/prompt_builder.c','src/agent/port_agent_prompt_builder.c'),
 ('src/ports/agent/conversation_loop.c','src/agent/conversation_loop.c','src/agent/port_agent_conversation_loop.c'),
 ('src/ports/agent/skill_commands.c','src/agent/skill_commands.c','src/agent/port_agent_skill_commands.c'),
 ('src/ports/agent/memory_provider.c','src/agent/memory_provider.c','src/agent/port_agent_memory_provider.c'),
 ('src/ports/agent/context_compressor.c','src/agent/context_compressor.c','src/agent/port_agent_context_compressor.c'),
 ('src/ports/agent/retry_utils.c','src/agent/retry_utils.c','src/agent/port_agent_retry_utils.c'),
 ('src/ports/agent/system_prompt.c','src/agent/system_prompt.c','src/agent/port_agent_system_prompt.c'),
 ('src/ports/agent/auxiliary_client.c','src/agent/auxiliary_client.c','src/agent/port_agent_auxiliary_client.c'),
 ('src/ports/agent/tool_executor.c','src/agent/tool_executor.c','src/agent/port_agent_tool_executor.c'),
 ('src/ports/agent/background_review.c','src/agent/background_review.c','src/agent/port_agent_background_review.c'),
 ('src/ports/agent/coding_context.c','src/agent/coding_context.c','src/agent/port_agent_coding_context.c'),
 ('src/ports/tools/mcp_tool.c','src/tools/mcp_tool.c','src/tools/port_mcp_tool.c'),
 ('src/ports/tools/approval.c','src/tools/approval.c','src/tools/port_approval.c'),
 ('src/ports/gateway/run.c','src/gateway/run.c','src/gateway/port_gateway_run.c'),
 ('src/ports/gateway/display_config.c','src/gateway/display_config.c','src/gateway/port_display_config.c'),
 # real clobbers (clean natives that are the real port; redundant port_ dup)
 ('src/hermes_cli/auth_helpers.c','src/cli/port_auth_helpers.c','src/cli/port_auth_helpers2.c'),
 ('src/agent/async_utils.c','src/agent/port_async_utils.c'),
 ('src/cron/scheduler.c','src/cron/port_scheduler.c'),
 ('src/cron/jobs.c','src/cron/port_jobs.c'),
]

def info(frel):
    rel = frel[len('src/'):] if frel.startswith('src/') else frel
    p = os.path.join(SRC, rel)
    if not os.path.exists(p):
        return (frel, 0, None, None)
    t = open(p).read()
    m = MARKER.search(t)
    pkg = mod = None
    if m:
        pkg, mod = m.group(1), m.group(2)
        pkg = PKG_FIX.get(pkg, pkg)
    n = t.count('\n') + 1
    return (frel, n, pkg, mod)

print(f'# MERGE-RESOLUTION TABLE — {len(SEED)} clusters (Pattern A dup + Pattern B redundant ports)')
print('# Rule: keep CANONICAL (clean name @ correct pkg folder); fold others in; delete.')
for grp in SEED:
    rows = [info(f) for f in grp]
    # canonical = file already at 1:1 target (clean name, no port_ prefix, correct pkg folder)
    # derive target from any marker present
    canon = None
    for (f, n, pkg, mod) in rows:
        if pkg and mod:
            target = f'src/{pkg}/{mod}.c'
            if f == target:
                canon = f
                break
    if canon is None:
        # pick largest
        canon = max(rows, key=lambda r: r[1])[0]
    print(f'\n## cluster: canonical = {canon}')
    for (f, n, pkg, mod) in sorted(rows, key=lambda r: -r[1]):
        tag = 'CANON' if f == canon else 'merge-in'
        pym = f'{pkg}/{mod}.py' if pkg else '?'
        print(f'   {tag:9s} {f:55s} {n:5d} lines  claims {pym}')
