#!/usr/bin/env python3
"""Validate Phase1 rename map WITHOUT moving anything."""
import os, re
COLLISION_SOURCES = {
 'agent/async_utils.c','agent/port_async_utils.c','cron/jobs.c','cron/port_jobs.c',
 'cron/scheduler.c','cron/port_scheduler.c','cli/config.c','cli/port_config.c',
 'cli/cli.c','cli/port_cli.c','agent/port_antigravity_oauth.c','agent/port_agent_antigravity_oauth.c',
 'agent/port_cron_health.c','agent/port_gateway_health.c','cli/port_voice.c','cli/port_hermes_cli_voice.c',
 'cli/port_config_helpers.c','cli/port_tools_config_helpers.c','cli/port_main.c','cli/main.c',
 'cli/port_hermes_cli_hooks.c','cli/port_gateway_hooks.c','cli/port_doctor.c',
 'cli/doctor.c','cli/port_anthropic_adapter.c','cli/port_agent_anthropic_adapter.c',
 'hermes_cli/port_agent_plugins_gaps.c','hermes_cli/port_plugins_gaps.c',
 'hermes_cli/port_service_manager_gaps.c','hermes_cli/port_hermes_cli_service_manager_gaps.c',
 'gateway/port_platforms_base.c','gateway/port_gateway_platforms_base.c',
 'gateway/port_gateway_run_gaps.c','gateway/port_run_gaps.c','cron/port_cron_jobs.c',
 'agent/config_py_final.c','agent/port_gateway_config_py_final.c','agent/port_hermes_cli_config_py_final.c',
 'agent/auxiliary_client.c','agent/port_auxiliary_client.c','agent/port_agent_auxiliary_client.c',
 'agent/auxiliary_client_gaps.c','agent/port_agent_auxiliary_client_gaps.c',
}

def new_rel(f):
    d, b = os.path.split(f)
    if d.startswith('src/ports/'):
        sub = d[len('src/ports/'):]; d2 = 'src/' + sub if sub else 'src'
    elif d == 'src/cli': d2 = 'src/hermes_cli'
    elif d.startswith('src/cli/'): d2 = 'src/hermes_cli/' + d[len('src/cli/'):]
    else: d2 = d
    if d2 == 'src/cli': d2 = 'src/hermes_cli'
    nb = re.sub(r'^port_(agent|tools|gateway|hermes_cli|cli|cron)_', '', b)
    nb = re.sub(r'^port_', '', nb)
    return os.path.join(d2, nb)

files = [os.path.relpath(os.path.join(dp, f), 'src') for dp, _, fs in os.walk('src') for f in fs if f.endswith('.c')]
moves = {}
collide = []
# A target is safe if no file OTHER than a rename-source currently occupies it.
all_sources = set(files)  # every .c currently present
for f in files:
    if f in COLLISION_SOURCES:
        continue
    np = new_rel(f)
    if np != f:
        if np in moves and moves[np] != f:
            collide.append((f, np, moves[np]))
        moves[np] = f
# Real clobber = target currently exists on disk AND the occupant is NOT a rename source (so it won't move away)
clobber_real = []
for t in set(moves.keys()):
    occ = os.path.join('src', t)
    if os.path.exists(occ):
        # who currently lives at t?
        # find a source f (non-collision) whose current path == t
        occupant_is_source = any(f == t for f in moves.values())
        if not occupant_is_source and t not in COLLISION_SOURCES:
            clobber_real.append(t)
print('non-collision renames:', len(moves))
print('distinct targets:', len(set(moves.keys())))
print('internal collisions (should be 0):', len(collide))
for c in collide: print('  COLL', c)
print('real clobbers (occupied by non-source):', len(clobber_real))
for c in clobber_real[:30]: print('  CLOB', c)
# Also report targets equal to their own source (no-op, fine)
noop = [f for f, np in moves.items() if f == np]
print('no-op (already clean):', len(noop))
