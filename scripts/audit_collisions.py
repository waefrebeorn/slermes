#!/usr/bin/env python3
import os, re
pairs = [
 'agent/async_utils.c','agent/port_async_utils.c',
 'cron/jobs.c','cron/port_jobs.c','cron/scheduler.c','cron/port_scheduler.c',
 'cli/config.c','cli/port_config.c','cli/cli.c','cli/port_cli.c',
 'agent/health.c','agent/port_gateway_health.c',
 'agent/antigravity_oauth.c','agent/port_antigravity_oauth.c',
 'agent/auxiliary_client.c','agent/port_auxiliary_client.c',
 'agent/port_antigravity_oauth.c','agent/port_agent_antigravity_oauth.c',
 'agent/port_cron_health.c','agent/port_gateway_health.c',
 'cli/port_voice.c','cli/port_hermes_cli_voice.c',
 'cli/port_config_helpers.c','cli/port_tools_config_helpers.c',
 'cli/port_main.c','cli/main.c','cli/port_hermes_cli_hooks.c','cli/port_gateway_hooks.c',
 'cli/port_doctor.c','cli/doctor.c','cli/port_anthropic_adapter.c','cli/port_agent_anthropic_adapter.c',
 'hermes_cli/port_agent_plugins_gaps.c','hermes_cli/port_plugins_gaps.c',
 'hermes_cli/port_service_manager_gaps.c','hermes_cli/port_hermes_cli_service_manager_gaps.c',
 'gateway/port_platforms_base.c','gateway/port_gateway_platforms_base.c',
 'gateway/port_gateway_run_gaps.c','gateway/port_run_gaps.c',
 'cron/port_cron_jobs.c','cron/jobs.c','cron/port_scheduler.c','cron/scheduler.c',
]
for f in pairs:
    p = os.path.join('src', f)
    if not os.path.exists(p):
        print(f'{f}: MISSING'); continue
    n = sum(1 for _ in open(p))
    # count function-ish definitions
    txt = open(p).read()
    funcs = len(re.findall(r'^[A-Za-z_][A-Za-z0-9_]*\s*\(', txt, re.M))
    # is it a real port or a native/legacy?
    is_port = 'Port of Python' in txt or 'port of python' in txt.lower() or 'PoP:' in txt or 'faithful' in txt.lower()
    print(f'{f}: {n} lines, ~{funcs} defs, port-marker={is_port}')
