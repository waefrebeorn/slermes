#!/usr/bin/env python3
"""gen_config_defaults_port.py — regenerate src/hermes_cli/config_defaults.c.

Faithful PoP of hermes_cli/config_defaults.py (pure-data leaf module):
DEFAULT_CONFIG and OPTIONAL_ENV_VARS. The tables are emitted verbatim as
canonical JSON text (ensure_ascii=False, compact separators — exactly what
json.dumps produces from the live Python module) embedded as C string
literals, parsed on demand through the repo's libjson so every consumer gets
a fresh json_t* tree with 1:1 key names and insertion order.

Usage: python3 scripts/gen_config_defaults_port.py [--quarry DIR]
"""
import argparse
import json
import os
import sys


def c_str_chunk(s: str) -> str:
    """Escape a Python string as one C double-quoted literal chunk."""
    out = ['"']
    for ch in s:
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\r':
            out.append('\\r')
        elif ch == '\t':
            out.append('\\t')
        elif ord(ch) < 0x20:
            out.append('\\x%02x' % ord(ch))
        else:
            out.append(ch)
    out.append('"')
    return ''.join(out)


def emit_table(f, c_name, doc, obj):
    text = json.dumps(obj, ensure_ascii=False, separators=(',', ':'))
    f.write('/* %s\n * Verbatim canonical JSON of %s.%s (generated — do not hand-edit;\n'
            ' * run scripts/gen_config_defaults_port.py to refresh).\n */\n'
            % (c_name, doc, c_name))
    f.write('static const char %s_JSON[] =' % c_name)
    # chunk so no single line exceeds ~120 columns; never split an escape
    line = ''
    chunks = []
    cur = ''
    for ch in text:
        cur += ch
        if len(cur) >= 96 and ch not in ('\\',) :
            # avoid ending a chunk right after a backslash (would split escape)
            chunks.append(cur)
            cur = ''
    if cur:
        chunks.append(cur)
    for i, chunk in enumerate(chunks):
        piece = c_str_chunk(chunk)
        if i % 4 == 0:
            if i:
                f.write('\n')
            f.write('    ')
        f.write('%s ' % piece)
    f.write(';\n\n')


HEADER = '''\
/*
 * config_defaults.c — PoP of hermes_cli/config_defaults.py (pure-data leaf).
 *
 * The Python module carries exactly two public names:
 *   DEFAULT_CONFIG       — the full default config.yaml table
 *   OPTIONAL_ENV_VARS    — optional environment-variable metadata
 * Both are ported 1:1 as canonical JSON text (generated verbatim from the
 * live Python tables by scripts/gen_config_defaults_port.py) parsed through
 * the repo libjson, preserving key names, nesting, insertion order, and the
 * None/True/False -> null/true/false mapping. Accessors return a FRESH tree;
 * the caller owns it and releases it with json_free().
 */
#define _POSIX_C_SOURCE 200809L
#include "config_defaults.h"
#include "hermes_json.h"

'''

FOOTER = '''
json_t *config_defaults_DEFAULT_CONFIG(void) {
    return json_parse(g_DEFAULT_CONFIG_JSON, NULL);
}

const char *config_defaults_DEFAULT_CONFIG_json(void) {
    return g_DEFAULT_CONFIG_JSON;
}

json_t *config_defaults_OPTIONAL_ENV_VARS(void) {
    return json_parse(g_OPTIONAL_ENV_VARS_JSON, NULL);
}

const char *config_defaults_OPTIONAL_ENV_VARS_json(void) {
    return g_OPTIONAL_ENV_VARS_JSON;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--quarry', default='/home/wubu/hermes-agent-dev')
    args = ap.parse_args()
    sys.path.insert(0, args.quarry)
    from hermes_cli.config_defaults import DEFAULT_CONFIG, OPTIONAL_ENV_VARS

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_c = os.path.join(root, 'src', 'hermes_cli', 'config_defaults.c')
    with open(out_c, 'w', encoding='utf-8') as f:
        f.write(HEADER)
        emit_table(f, 'g_DEFAULT_CONFIG', 'hermes_cli/config_defaults.py',
                   DEFAULT_CONFIG)
        emit_table(f, 'g_OPTIONAL_ENV_VARS', 'hermes_cli/config_defaults.py',
                   OPTIONAL_ENV_VARS)
        f.write(FOOTER)
    print('wrote %s' % out_c)


if __name__ == '__main__':
    main()
