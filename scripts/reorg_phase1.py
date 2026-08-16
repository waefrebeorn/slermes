#!/usr/bin/env python3
"""Phase 1: mechanical, collision-free 1:1 rename of slermes src/.

- Drops port_ (and port_<pkg>_) prefixes from .c AND .h files.
- Fixes folders: src/ports/<pkg> -> src/<pkg>; src/cli -> src/hermes_cli.
- Rewrites #include "port_*.h" -> new name (word-boundary).
- Rewrites build/objects.mk paths.
- Skips the 23 known collision targets (handled in Phase 2).

Idempotent-ish: only acts on files currently matching a port_ prefix or in
src/ports or src/cli. Re-run after Phase 2 to catch stragglers is safe.
"""
import os
import re
import shutil

ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
OBJ_MK = os.path.join(ROOT, 'build', 'objects.mk')

# ---- collision targets (do NOT auto-rename; Phase 2 merges them) ----
COLLISION_SOURCES = {
 'agent/async_utils.c','agent/port_async_utils.c',
 'cron/jobs.c','cron/port_jobs.c','cron/scheduler.c','cron/port_scheduler.c',
 'cli/config.c','cli/port_config.c','cli/cli.c','cli/port_cli.c',
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
 'cron/port_cron_jobs.c',
}

def new_rel(f):
    d, b = os.path.split(f)
    if d.startswith('src/ports/'):
        sub = d[len('src/ports/'):]
        d2 = 'src/' + sub if sub else 'src'
    elif d == 'src/cli':
        d2 = 'src/hermes_cli'
    elif d.startswith('src/cli/'):
        d2 = 'src/hermes_cli/' + d[len('src/cli/'):]
    else:
        d2 = d
    if d2 == 'src/cli':
        d2 = 'src/hermes_cli'
    nb = re.sub(r'^port_(agent|tools|gateway|hermes_cli|cli|cron)_', '', b)
    nb = re.sub(r'^port_', '', nb)
    return os.path.join(d2, nb)

# Collect all .c and .h
allfiles = []
for ext in ('.c', '.h'):
    for dp, _, fs in os.walk(SRC):
        for fn in fs:
            if fn.endswith(ext):
                allfiles.append(os.path.relpath(os.path.join(dp, fn), ROOT))

# Build move map (exclude collisions and files whose new name equals old)
moves = {}
for f in allfiles:
    if f in COLLISION_SOURCES:
        continue
    np = new_rel(f)
    if np != f:
        moves[f] = np

# Also fix includes referencing moved headers: build old->new header map
hdr_map = {}
for f in allfiles:
    if f.endswith('.h') and f not in COLLISION_SOURCES:
        np = new_rel(f)
        if np != f:
            hdr_map[os.path.basename(f)] = os.path.basename(np)

print(f'Phase1 renames: {len(moves)}')
print(f'Phase1 header include rewrites: {len(hdr_map)}')

# Apply renames (mkdir target dirs first)
renamed = []
for f, np in moves.items():
    srcp = os.path.join(ROOT, f)
    dstp = os.path.join(ROOT, np)
    os.makedirs(os.path.dirname(dstp), exist_ok=True)
    if os.path.exists(dstp):
        print('SKIP EXISTING TARGET', dstp)
        continue
    shutil.move(srcp, dstp)
    renamed.append((f, np))

# Rewrite #include "X.h" inside all moved .c/.h to new basename
inc_re = re.compile(r'#include\s+"([^"]+\.h)"')
count_inc = 0
for f, np in renamed:
    p = os.path.join(ROOT, np)
    if not (np.endswith('.c') or np.endswith('.h')):
        continue
    try:
        txt = open(p).read()
    except Exception:
        continue
    def repl(m):
        base = m.group(1).split('/')[-1]
        if base in hdr_map:
            return '#include "' + hdr_map[base] + '"'
        return m.group(0)
    newtxt = inc_re.sub(repl, txt)
    if newtxt != txt:
        open(p, 'w').write(newtxt)
        count_inc += 1
print(f'files with rewritten includes: {count_inc}')

# Rewrite objects.mk
mk = open(OBJ_MK).read()
mk_new = mk
for f, np in renamed:
    mk_new = mk_new.replace(f, np)
# also rewrite any include paths that reference src/ports or src/cli roots
mk_new = mk_new.replace('src/ports/', 'src/').replace('src/cli/', 'src/hermes_cli/')
if mk_new != mk:
    open(OBJ_MK, 'w').write(mk_new)
    print('objects.mk rewritten')
else:
    print('objects.mk: no changes')

print('Phase1 done. Renamed', len(renamed), 'files.')
