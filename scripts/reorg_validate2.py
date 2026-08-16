#!/usr/bin/env python3
"""Validate Phase1 rename: marker-driven, 1:1 Python-package routing.

For each port_*.c/.h in src/, determine the AUTHORITATIVE Python module from
its fold-in marker ('Port of Python <pkg>/<mod>.py'); route the file to
src/<pkg>/<mod>.c (stripping port_/port_<pkg>_ prefix). Files without a marker
fall back to their current folder (with cli->hermes_cli fix).

Reports: rename count, collisions (two sources -> same target), real clobbers
(target occupied by a non-source file), and the 56 mis-foldered cases.
"""
import os
import re

ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')

MARKER = re.compile(r'Port of Python\s+([a-z_]+)/([A-Za-z0-9_]+)\.py', re.I)
PKG_FIX = {'hermes_cli': 'hermes_cli', 'cli': 'hermes_cli'}

COLLISION_SOURCES = {
 'agent/async_utils.c','agent/port_async_utils.c','cron/jobs.c','cron/port_jobs.c',
 'cron/scheduler.c','cron/port_scheduler.c','cli/config.c','cli/port_config.c',
 'cli/cli.c','cli/port_cli.c','agent/port_antigravity_oauth.c','agent/port_agent_antigravity_oauth.c',
 'agent/port_cron_health.c','agent/port_gateway_health.c','cli/port_voice.c','cli/port_hermes_cli_voice.c',
 'cli/port_config_helpers.c','cli/port_tools_config_helpers.c','cli/port_main.c','cli/main.c',
 'cli/port_hermes_cli_hooks.c','cli/port_gateway_hooks.c','cli/port_doctor.c','cli/doctor.c',
 'cli/port_anthropic_adapter.c','cli/port_agent_anthropic_adapter.c',
 'hermes_cli/port_agent_plugins_gaps.c','hermes_cli/port_plugins_gaps.c',
 'hermes_cli/port_service_manager_gaps.c','hermes_cli/port_hermes_cli_service_manager_gaps.c',
 'gateway/port_platforms_base.c','gateway/port_gateway_platforms_base.c',
 'gateway/port_gateway_run_gaps.c','gateway/port_run_gaps.c','cron/port_cron_jobs.c',
 'agent/config_py_final.c','agent/port_gateway_config_py_final.c','agent/port_hermes_cli_config_py_final.c',
 'agent/auxiliary_client.c','agent/port_auxiliary_client.c','agent/port_agent_auxiliary_client.c',
 'agent/auxiliary_client_gaps.c','agent/port_agent_auxiliary_client_gaps.c',
 'cli/display.c',
}

def strip_prefix(b):
    nb = re.sub(r'^port_(agent|tools|gateway|hermes_cli|cli|cron)_', '', b)
    nb = re.sub(r'^port_', '', nb)
    return nb

def target_for(frel):
    p = os.path.join(SRC, frel)
    try:
        t = open(p).read()
    except Exception:
        t = ''
    m = MARKER.search(t)
    base = os.path.basename(frel)
    if m:
        pkg, mod = m.group(1), m.group(2)
        pkg = PKG_FIX.get(pkg, pkg)
        # filename's mod part may differ (e.g. port_agent_foo.c); prefer marker mod
        return os.path.join('src', pkg, mod + '.c')
    # fallback: current folder, fix cli->hermes_cli
    d, b = os.path.split(frel)
    if d == 'src/cli':
        d2 = 'src/hermes_cli'
    elif d.startswith('src/cli/'):
        d2 = 'src/hermes_cli/' + d[len('src/cli/'):]
    elif d.startswith('src/ports/'):
        sub = d[len('src/ports/'):]
        d2 = 'src/' + sub if sub else 'src'
    else:
        d2 = d
    if d2 == 'src/cli':
        d2 = 'src/hermes_cli'
    return os.path.join(d2, strip_prefix(b))

allfiles = []
for ext in ('.c', '.h'):
    for dp, _, fs in os.walk(SRC):
        for fn in fs:
            if fn.endswith(ext):
                allfiles.append(os.path.relpath(os.path.join(dp, fn), ROOT))

moves = {}
collide = []
misfolder = []
for f in allfiles:
    if f in COLLISION_SOURCES:
        continue
    if 'port_' not in os.path.basename(f) and not (f.startswith('src/ports/') or f.startswith('src/cli/')):
        # native file not needing rename (e.g. src/agent/async_utils.c) — skip unless it's a collision source
        continue
    np = target_for(f)
    if np == f:
        continue
    if np in moves and moves[np] != f:
        collide.append((f, np, moves[np]))
    moves[np] = f

# misfoldered: target pkg != current top folder
for f, np in moves.items():
    cur_top = f.split('/')[1] if f.startswith('src/') else ''
    tgt_top = np.split('/')[1] if np.startswith('src/') else ''
    cur_top = PKG_FIX.get(cur_top, cur_top)
    if cur_top != tgt_top:
        misfolder.append((f, np))

# real clobbers
clobber = []
for t in set(moves.keys()):
    occ = os.path.join(ROOT, t)
    if os.path.exists(occ):
        occupant_is_source = any(f == t for f in moves.values())
        if not occupant_is_source and t not in COLLISION_SOURCES:
            clobber.append(t)

print('rename moves (marker-driven):', len(moves))
print('internal collisions (should be 0):', len(collide))
for c in collide: print('  COLL', c)
print('mis-foldered (marker pkg != folder):', len(misfolder))
print('real clobbers (occupied by non-source):', len(clobber))
for c in clobber[:30]: print('  CLOB', c)
