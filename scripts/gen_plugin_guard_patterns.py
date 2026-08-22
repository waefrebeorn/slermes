#!/usr/bin/env python3
"""gen_plugin_guard_patterns.py — regenerate src/tools/plugin_guard_patterns.h.

Extracts THREAT_PATTERNS / SCANNABLE_EXTENSIONS / SUSPICIOUS_BINARY_EXTENSIONS /
INVISIBLE_CHARS verbatim from the live tools/skills_guard.py so the C
plugin_guard port scans with the identical pattern set. Patterns using
lookahead/lookbehind ((?= (?! (?<= (?<!) — unsupported by the repo libregex
POSIX backend) are emitted with the PG_PATTERN_SKIPPED flag, exactly like the
Python-side engine would never see them compiled on this backend.
"""
import json
import re
import sys


def main():
    rows = []
    py = open('/home/wubu/hermes-agent-dev/tools/skills_guard.py',
              encoding='utf-8').read()
    pat = re.compile(
        r"\(r'((?:[^'\\]|\\.)*)',\s*\"([a-z0-9_]+)\",\s*\"(\w+)\","
        r"\s*\"([a-z_]+)\",\s*\n?\s*\"((?:[^\"\\]|\\.)*)\"\)", re.S)
    for regex_s, pid, sev, cat, desc in pat.findall(py):
        rows.append((regex_s, pid, sev, cat, desc))
    assert len(rows) >= 100, len(rows)

    out = []
    out.append('/*\n'
               ' * plugin_guard_patterns.h — GENERATED from tools/skills_guard.py\n'
               ' * THREAT_PATTERNS (do not hand-edit;\n'
               ' * run scripts/gen_plugin_guard_patterns.py to refresh).\n'
               ' * Patterns whose regex needs lookahead/lookbehind — unsupported\n'
               ' * by the repo POSIX libregex backend — carry PG_SKIP and are\n'
               ' * skipped by the scanner at runtime.\n'
               ' */\n')
    out.append('#define PG_PATTERN_FLAGS_OK  0\n#define PG_PATTERN_FLAGS_SKIP 1\n')
    out.append('typedef struct {\n'
               '    const char *pattern;\n'
               '    const char *id;\n'
               '    const char *severity;\n'
               '    const char *category;\n'
               '    const char *description;\n'
               '    int flags;\n'
               '} pg_threat_pattern_t;\n')
    out.append('static const pg_threat_pattern_t PG_THREAT_PATTERNS[] = {')

    def c_escape(s):
        outp = ''
        for ch in s:
            if ch == '\\':
                outp += '\\\\'
            elif ch == '"':
                outp += '\\"'
            elif ch == '\n':
                outp += '\\n'
            else:
                outp += ch
        return outp

    for regex_s, pid, sev, cat, desc in rows:
        # unescape python single-quoted raw string -> literal regex text
        rx = regex_s.replace("\\'", "'")
        skip = ('(?=' in rx) or ('(?!' in rx) or ('(?<' in rx)
        flags = 'PG_PATTERN_FLAGS_SKIP' if skip else 'PG_PATTERN_FLAGS_OK'
        out.append('    {%s, %s, %s, %s, %s, %s},'
                   % (json.dumps(rx), json.dumps(pid), json.dumps(sev),
                      json.dumps(cat), json.dumps(desc), flags))

    def cstr_set(name, items):
        body = ', '.join(json.dumps(i) for i in sorted(items))
        return ('static const char *const %s[] = {%s, NULL};\n'
                % (name, body))

    # sets: parse literal blocks from the source
    def parse_set(name):
        i = py.find(name + ' = {')
        j = py.find('}', i)
        return set(re.findall(r"'([^']+)'", py[i:j]))

    scannable = parse_set('SCANNABLE_EXTENSIONS')
    binary = parse_set('SUSPICIOUS_BINARY_EXTENSIONS')
    inv_raw = parse_set('INVISIBLE_CHARS')
    # entries are python escapes like '\u200b'
    inv = []
    for tok in inv_raw:
        inv.append(tok.encode().decode('unicode_escape'))
    out.append('    {NULL, NULL, NULL, NULL, NULL, PG_PATTERN_FLAGS_OK}')
    out.append('};')
    out.append(cstr_set('PG_SCANNABLE_EXTENSIONS', scannable))
    out.append(cstr_set('PG_SUSPICIOUS_BINARY_EXTENSIONS', binary))

    inv_tbl = ['static const struct { unsigned cp; const char *name; '
               'const char *label; } PG_INVISIBLE_CHARS[] = {']
    names = {
        0x200b: 'zero-width space', 0x200c: 'zero-width non-joiner',
        0x200d: 'zero-width joiner', 0x2060: 'word joiner',
        0x2062: 'invisible times', 0x2063: 'invisible separator',
        0xfeff: 'zero-width no-break space',
    }
    for cp in inv:
        nm = names.get(ord(cp), 'invisible character')
        inv_tbl.append('    {0x%04x, "%s"},' % (ord(cp), nm))
    inv_tbl.append('    {0, NULL}')
    out.append('\n'.join(inv_tbl) + '\n};')

    dst = '/home/wubu/hermes-agent-dev/slermes/src/tools/plugin_guard_patterns.h'
    with open(dst, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out) + '\n')
    nskip = sum(1 for r in rows if '(?' in r[0])
    print('wrote %s: %d patterns (%d skipped-lookaround)' % (dst, len(rows), nskip))


if __name__ == '__main__':
    main()
