#!/usr/bin/env python3
"""Auto-port constants-only Python modules into real, differentially gated C11.

For every module classified `consts_only` in docs/gap-complexity.json, emit:
  src/<mod>.c        <gate>_constants() building a json_t of all literal top-level constants
  include/noc_<gate>.h
  tests/t_port_<gate>.c    generic fixture -> {"func":"constants","out":...} harness
  tests/oracle_<gate>.py   imports LIVE module, compares constants as JSON-compatible values
  tests/fixtures/<gate>.jsonl
  docs/wave-const-map.json  gate -> module map for scripts/gate_wave.py
"""
import ast, hashlib, json, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUARRY = 'C:/Users/eman5/hermes-agent'


def cstr(s):
    out = s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t')
    return '"' + out + '"'


def cnode(v, indent=0):
    """Return C expression (string) building a json_t for a Python literal."""
    pad = ' ' * indent
    if v is None:
        return 'json_null()'
    if isinstance(v, bool):
        return 'json_bool(%s)' % ('true' if v else 'false')
    if isinstance(v, int):
        if abs(v) > 9007199254740992:
            return 'json_string(%s)' % cstr(str(v))
        return 'json_number(%s)' % v
    if isinstance(v, float):
        return 'json_number(%.17g)' % v
    if isinstance(v, str):
        return 'json_string(%s)' % cstr(v)
    if isinstance(v, (list, tuple, set, frozenset)):
        items = list(v)
        if isinstance(v, (set, frozenset)):
            items = sorted(items, key=lambda x: repr(x))
        lines = ['(json_t *)NULL']  # placeholder, replaced below
        parts = []
        for it in items:
            parts.append((pad + '    json_append(__a, %s);' % cnode(it, indent + 4)))
        return '({ json_t *__a = json_array();\n' + '\n'.join(parts) + '\n' + pad + '  __a; })'
    if isinstance(v, dict):
        parts = []
        for k in sorted(v.keys(), key=lambda x: str(x)):
            parts.append(pad + '    json_set(__o, %s, %s);' % (cstr(str(k)), cnode(v[k], indent + 4)))
        return '({ json_t *__o = json_object();\n' + '\n'.join(parts) + '\n' + pad + '  __o; })'
    return 'json_null()'


def py_compat(v):
    """Convert a Python literal into a JSON-comparable value matching cnode()."""
    if isinstance(v, (set, frozenset)):
        return [py_compat(x) for x in sorted(v, key=lambda x: repr(x))]
    if isinstance(v, (list, tuple)):
        return [py_compat(x) for x in v]
    if isinstance(v, dict):
        return {str(k): py_compat(vv) for k, vv in v.items()}
    if isinstance(v, int) and abs(v) > 9007199254740992:
        return str(v)
    return v


def literal_ext(node):
    """ast.literal_eval plus frozenset/set/dict constructor calls."""
    import ast as _ast
    try:
        return _ast.literal_eval(node)
    except Exception:
        pass
    if isinstance(node, _ast.Call) and isinstance(node.func, _ast.Name):
        name = node.func.id
        if name in ('frozenset', 'set', 'tuple', 'list') and len(node.args) == 1:
            try:
                inner = _ast.literal_eval(node.args[0])
            except Exception:
                return None
            if name == 'frozenset': return frozenset(inner)
            if name == 'set': return set(inner)
            if name == 'tuple': return tuple(inner)
            return list(inner)
        if name == 'dict' and not node.args and node.keywords:
            out = {}
            for kw in node.keywords:
                try: out[kw.arg] = _ast.literal_eval(kw.value)
                except Exception: pass
            return out if out else None
    return None


def make_gate(mod):
    h = hashlib.md5(mod.encode('utf-8')).hexdigest()[:10]
    return 'cn_' + h


def main():
    cats = json.load(open(os.path.join(ROOT, 'docs', 'gap-complexity.json'), encoding='utf-8'))
    mapping = {}
    generated = skipped = 0
    for mod in cats['consts_only']:
        src = os.path.join(QUARRY, mod + '.py')
        if not os.path.isfile(src):
            skipped += 1; continue
        try:
            tree = ast.parse(open(src, encoding='utf-8', errors='ignore').read())
        except Exception:
            skipped += 1; continue
        consts = []
        for n in tree.body:
            targets = []
            if isinstance(n, ast.Assign):
                targets = [t.id for t in n.targets if isinstance(t, ast.Name)]
                val = n.value
            elif isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name):
                targets = [n.target.id]; val = n.value
            else:
                continue
            if not val:
                continue
            value = literal_ext(val)
            if value is None:
                continue
            for name in targets:
                if not name.lstrip('_').isupper():
                    continue
                consts.append((name, value))
        if not consts:
            skipped += 1; continue

        gate = make_gate(mod)
        ident = re.sub(r'[^A-Za-z0-9_]', '_', mod.replace('/', '_'))
        c_lines = [
            '/*',
            ' * %s.c — real C port of upstream Hermes %s.py constant tables.' % (mod.split('/')[-1], mod),
            ' * PoP: %s_constants @ %s.py:<module-constants>' % (gate, mod),
            ' * Auto-generated from literal top-level constants; gated against live Python.',
            ' */',
            '#include "hermes_json.h"',
            '#include "noc_%s.h"' % gate,
            '',
            'json_t *%s_constants(void) {' % gate,
            '    json_t *out = json_object();',
        ]
        for name, value in consts:
            c_lines.append('    json_set(out, %s, %s);' % (cstr(name), cnode(value, 4)))
        c_lines += ['    return out;', '}', '']
        hdr = ('#ifndef NOC_%s_H\n#define NOC_%s_H\nstruct json_t;\n'
               'struct json_t *%s_constants(void);\n#endif\n' % (gate.upper(), gate.upper(), gate))
        harness = '''#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include "noc_%s.h"
int main(void) {
    char line[8192];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *out = %s_constants();
        char *s = json_dumps(out, 0);
        printf("{\\"func\\":\\"constants\\",\\"out\\":%%s}\\n", s ? s : "null");
        free(s); json_free(out); json_free(in);
    }
    return 0;
}
''' % (gate, gate)
        oracle = '''#!/usr/bin/env python3
"""Oracle for literal constants of %s.py."""
import importlib.util, json, os, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
SRC = "C:/Users/eman5/hermes-agent/%s.py"
def _load_live():
    parts = %r.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i]); pdir = os.path.join(Q, *parts[:i])
        if pname in sys.modules: continue
        sp = importlib.util.spec_from_file_location(pname, os.path.join(pdir, "__init__.py"), submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp); sys.modules[pname] = pm
        try: sp.loader.exec_module(pm)
        except Exception: pass
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name, SRC)
    live = importlib.util.module_from_spec(sp); sys.modules[name] = live
    sp.loader.exec_module(live); return live
live = _load_live()
def compat(v):
    if isinstance(v, (set, frozenset)): return [compat(x) for x in sorted(v, key=repr)]
    if isinstance(v, (list, tuple)): return [compat(x) for x in v]
    if isinstance(v, dict): return {str(k): compat(x) for k, x in v.items()}
    if isinstance(v, int) and abs(v) > 9007199254740992: return str(v)
    return v
names = %s
py = {n: compat(getattr(live, n)) for n in names}
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") != "constants": continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH %s py={py!r} c={got!r}", file=sys.stderr)
print("ORACLE %s: %%d cases, %%d mismatches" %% (cases, mism))
sys.exit(1 if mism else 0)
''' % (mod, mod, gate, json.dumps([n for n, _ in consts]), mod, gate, gate)

        for rel, text in (
            ('src/' + mod + '.c', '\n'.join(c_lines)),
            ('include/noc_%s.h' % gate, hdr),
            ('tests/t_port_%s.c' % gate, harness),
            ('tests/oracle_%s.py' % gate, oracle),
            ('tests/fixtures/%s.jsonl' % gate, '{"func":"constants"}\n'),
        ):
            p = os.path.join(ROOT, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, 'w', encoding='utf-8', newline='').write(text)
        mapping[gate] = mod
        generated += 1
        if generated % 25 == 0:
            print('generated', generated, flush=True)
    json.dump(mapping, open(os.path.join(ROOT, 'docs', 'wave-const-map.json'), 'w', encoding='utf-8'), indent=1)
    print('DONE generated=%d skipped=%d' % (generated, skipped))


if __name__ == '__main__':
    main()
