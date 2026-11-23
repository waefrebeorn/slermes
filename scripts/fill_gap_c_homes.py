#!/usr/bin/env python3
"""Phase 1 of gap closure: create an honest C HOME for every remaining Python gap.

These are NOT verified ports. Each file states STATUS: C HOME CREATED — behavior
port pending. Simple module-level literal constants are extracted; complex logic
is explicitly marked pending. Nothing here is wired into the C build: the parent
wires and gates real ports separately. PoP annotations are provenance, not parity.
"""
import json, os, re, sys

REPO = r'C:/Users/eman5/slermes'
QUARRY = r'C:/Users/eman5/hermes-agent'
GAPJSON = os.path.join(REPO, 'docs/organization-parity.json')

# Real ports already landed or in flight — never overwrite them.
HANDLED = set('''
acp_adapter/modes
hermes_cli/config_turn_limit
hermes_cli/url_utils
cron/jobs_tristate
cron/constants
gateway/run_common
tools/tool_output_truncate
hermes_cli/toolset_scope
gateway/platforms/base_media_flags
hermes_cli/config_key_notices
gateway/platforms/base_image_sniff
tools/process_registry_wait
hermes_cli/models_process_fingerprint
tui_gateway/_env
hermes_cli/config_issues
gateway/bot_peer_notices
persistence/sqlite/conversation/_store/local_migration
agent/bedrock_service_tiers
hermes_cli/shared_profile_warning
cron/env_settings
gateway/session_tool_events
hermes_cli/colors
session_rules/surfaces
tools/clarify_batch
hermes_cli/plugin_compat
gateway/runtime_contract
hermes_cli/models_fast_regions
hermes_cli/cli_panel
agent/auxiliary_profile_gates
agent/credential_pool_timestamps
hermes_cli/profiles_config
tools/environments/path_utils
gateway/session_skin
hermes_cli/subcommands/bundles
hermes_cli/subcommands/checkpoints
hermes_cli/subcommands/completion
hermes_cli/subcommands/moa
tui_gateway/connector_payload
hermes_cli/cli_tui_wrap
gateway/hosted_room_task_input
persistence/core/canonical
persistence/core/keys
gateway/session_bot_state
'''.split())

BS = chr(92)
LIT = re.compile(r'^([A-Z][A-Z0-9_]*)\s*=\s*(.+?)\s*$')


def cident(s):
    return re.sub(r'[^A-Za-z0-9_]', '_', s)


def literal(value):
    """Return a C literal for a simple Python literal, or None."""
    v = value.strip()
    if v.startswith('#'):
        return None
    if re.fullmatch(r'-?\d+', v):
        return v
    if re.fullmatch(r'(?i)true|false', v):
        return '1' if v.lower() == 'true' else '0'
    if len(v) >= 2 and v[0] == v[-1] and v[0] in ('"', "'"):
        inner = v[1:-1].replace('\\', '\\\\').replace('"', '\\"')
        return '"' + inner + '"'
    return None


def make_home(mod, src_text):
    ident = cident(mod.replace('/', '_'))
    consts = []
    for line in src_text.splitlines():
        m = LIT.match(line)
        if not m:
            continue
        lit = literal(m.group(2))
        if lit is None:
            continue
        consts.append('static const char *const %s_CONST_%s = %s;' %
                      (ident, m.group(1), lit))
    body = [
        '/*',
        ' * %s.c — C home for upstream Hermes %s.py.' % (mod.split('/')[-1], mod),
        ' * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,',
        ' *          not verified parity. It is deliberately not linked into the build.',
        ' * PoP: %s_pending @ %s.py:<module>' % (ident, mod),
        ' */',
        '#include <stddef.h>',
        '',
    ]
    if consts:
        body.append('/* Module-level literal constants mirrored from the Python source. */')
        body.extend(consts)
        body.append('')
    body += [
        'void %s_port_pending(void) { (void)0; }' % ident,
        '',
    ]
    return '\n'.join(body)


def main():
    d = json.load(open(GAPJSON, encoding='utf-8'))
    gaps = [g.replace(BS, '/') for g in d['gap']]
    made = skipped = 0
    manifest = []
    for mod in gaps:
        if mod in HANDLED:
            skipped += 1
            continue
        cpath = os.path.join(REPO, 'src', mod + '.c')
        if os.path.isfile(cpath):
            skipped += 1
            continue
        pypath = os.path.join(QUARRY, mod + '.py')
        try:
            text = open(pypath, encoding='utf-8', errors='ignore').read()
        except OSError:
            text = ''
        os.makedirs(os.path.dirname(cpath), exist_ok=True)
        with open(cpath, 'w', encoding='utf-8', newline='') as fh:
            fh.write(make_home(mod, text))
        made += 1
        manifest.append(mod)
        if made % 250 == 0:
            print('created', made, flush=True)
    json.dump(manifest, open(os.path.join(REPO, 'docs', 'gap-c-homes.json'), 'w', encoding='utf-8'),
              indent=1)
    print('DONE created=%d skipped=%d' % (made, skipped))


if __name__ == '__main__':
    main()
