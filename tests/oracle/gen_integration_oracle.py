#!/usr/bin/env python3
"""gen_integration_oracle.py — integration-oracle generator for the runtime/
integration modules that gen_oracle.py (scalar-only heuristic) skips.

Contract (copied from tests/t_port_account_usage.c / sta_oracle_account_usage.py,
which the generic runner tests/oracle/runners/run_oracle.sh diffs byte-for-byte):

  * Harness  tests/t_port_<mod>.c  reads a JSON ARRAY fixture from argv[1];
    each element is {"op":<cfunc>, "value":<input-string>}. It dispatches by
    "op", calls the ported C function with value, and prints one
    {"fn":<op>, "out":<result>} JSON line per case.
  * Oracle   tests/sta_oracle_<mod>.py  reads the SAME fixture from argv[1],
    recomputes each case from the LIVE Python source, and prints the same shape.
  * The runner links both against the real `slermes` object closure (with
    -Wl,--allow-multiple-definition) and diffs them.

Scope of this generator: PURE functions of the shape
        ret  foo(const char *arg) { ... no socket/sqlite/popen/getenv ... }
that have a PoP annotation mapping them to a real Python function. These are
mechanically oracle-able: feed the same input string to Python and C, diff. No
mocked network/DB is needed because the function is pure - the "infrastructure"
is just the input string.

Usage:
    python3 gen_integration_oracle.py <port_module.c> [--write]
Scans for pure (const char *) -> char*/bool/int functions whose PoP annotation
maps to a real Python function, and (with --write) emits harness + oracle +
fixture, then registers into registry.json.
"""
import os, re, sys, json

SL = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

SIG_RE = re.compile(r"([\w]+(?:\s*\*)?)\s+(\w+)\s*\(([^)]*)\)\s*\{", re.S)
POP_RE = re.compile(r"PoP:\s*(\w+)\s+@\s+([\w/]+\.py)(?::(\w+))?")
IO_BLOCK = re.compile(r"\b(fopen|fread|fwrite|popen|system|socket|connect|sqlite|curl|recv|send|getenv|setenv|read_file|write_file|http_|mcp_|fork|execv|dlopen)\b")

SIMPLE_RET = {"char*", "int", "long", "size_t", "unsigned", "uint",
              "uint8_t", "uint32_t", "uint64_t", "int8_t", "int32_t", "int64_t", "bool"}

FIXTURES = [
    "", "x", "hello world", '{"a":1,"b":[2,3]}', '[{"id":1},{"id":2}]',
    "user@example.com", "https://example.com/path?q=1", "line1\nline2\nline3",
    "/abs/path/to/file.txt", "CamelCaseString", "12345", "  spaced  ",
    "<html><body>hi</body></html>", "a=b&pwd=secret&token=x", "key:value;key2:value2",
]


def find_cfile(py):
    base = os.path.basename(py)[:-3]
    for root in ("src/agent", "src/cli", "src/tools", "src/gateway", "src/cron", "src"):
        c = os.path.join(SL, root, "port_" + base + ".c")
        if os.path.isfile(c):
            return c
        c = os.path.join(SL, root, base + ".c")
        if os.path.isfile(c):
            return c
    return None


def parse_sigs(cfile):
    txt = open(cfile, encoding="utf-8", errors="replace").read()
    d = {}
    for m in SIG_RE.finditer(txt):
        d[m.group(2)] = (m.group(1).strip(), m.group(3).strip())
    return d


def extract_body(cfile, func):
    txt = open(cfile, encoding="utf-8", errors="replace").read()
    idx = txt.find(func + "(")
    if idx < 0:
        return ""
    bo = txt.find("{", idx)
    if bo < 0:
        return ""
    depth = 0
    for j in range(bo, len(txt)):
        if txt[j] == "{":
            depth += 1
        elif txt[j] == "}":
            depth -= 1
            if depth == 0:
                return txt[bo + 1:j]
    return ""


def candidate_funcs(cfile):
    """Return list of (cfunc, pymod, pyfunc) for pure (const char*) ports."""
    txt = open(cfile, encoding="utf-8", errors="replace").read()
    sigs = parse_sigs(cfile)
    pops_all = POP_RE.findall(txt)
    primary = None
    if pops_all:
        from collections import Counter
        mods = Counter(p[1][:-3].replace("/", ".") for p in pops_all if "@" in p[0] or p[1])
        if mods:
            primary = mods.most_common(1)[0][0]
    out = []
    for fn, (ret, params) in sigs.items():
        if ret not in SIMPLE_RET:
            continue
        p = params.strip()
        if not re.match(r"^\s*const\s+char\s*\*\s*\w+\s*$", p) and \
           not re.match(r"^\s*char\s*\*\s*\w+\s*$", p):
            continue
        if fn.startswith("port_") or fn in ("main", "free", "malloc"):
            continue
        idx = txt.find(fn + "(")
        pre = txt[:idx].rstrip().splitlines()[-1] if idx > 0 else ""
        if re.search(r"\bstatic\b", pre):
            continue
        chunk = txt[:idx]
        last_pop = None
        for m in POP_RE.finditer(chunk):
            last_pop = m
        if not last_pop:
            continue
        pymod = (last_pop.group(2)[:-3].replace("/", ".")) if "@" in last_pop.group(0) else primary
        if not pymod:
            continue
        pyfn = last_pop.group(3) or last_pop.group(1)
        body = extract_body(cfile, fn)
        if IO_BLOCK.search(body):
            continue
        out.append((fn, pymod, pyfn))
    return out


def gen_harness(mod, cfile, funcs, sigs):
    hdr = cfile[:-2] + ".h"
    inc = os.path.basename(hdr) if os.path.isfile(hdr) else os.path.basename(cfile)
    L = []
    L.append("/* AUTO-GENERATED integration oracle harness for %s (gen_integration_oracle.py). */" % mod)
    L.append('#include "hermes_core_types.h"')
    L.append('#include "hermes_json.h"')
    L.append('#include "%s"' % inc)
    L.append("#include <stdio.h>")
    L.append("#include <stdlib.h>")
    L.append("#include <string.h>")
    L.append("")
    # self-declare tested functions (some ports don't export them in the header)
    for fn, _, _ in funcs:
        ret = sigs[fn][0]
        L.append("extern %s %s(const char *);" % (ret, fn))
    L.append("")
    L.append("static char *read_all(const char *path){")
    L.append("    FILE *f = fopen(path, \"rb\"); if (!f) return NULL;")
    L.append("    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);")
    L.append("    if (n < 0) { fclose(f); return NULL; }")
    L.append("    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }")
    L.append("    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\\0'; fclose(f); return buf;")
    L.append("}")
    L.append("")
    # emit functions per func
    for fn, _, _ in funcs:
        ret = sigs[fn][0]
        L.append("static json_t *emit_%s(const json_t *c){" % fn)
        L.append('    const char *value = json_get_str(c, "value", "");')
        if ret == "char*":
            L.append("    const char *out = %s(value);" % fn)
            L.append('    json_t *o = json_new_object(); json_set(o, "fn", json_string("%s"));' % fn)
            L.append('    json_set(o, "out", json_string(out ? out : "")); return o;')
        elif ret == "bool":
            L.append("    int v = (int)%s(value);" % fn)
            L.append('    json_t *o = json_new_object(); json_set(o, "fn", json_string("%s"));' % fn)
            L.append('    json_set(o, "out", json_bool(v)); return o;')
        else:  # int family
            L.append("    long v = (long)%s(value);" % fn)
            L.append('    json_t *o = json_new_object(); json_set(o, "fn", json_string("%s"));' % fn)
            L.append('    json_set(o, "out", json_int(v)); return o;')
        L.append("}")
        L.append("")
    L.append("int main(int argc, char **argv){")
    L.append('    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\\n", argv[0]); return 2; }')
    L.append("    char *input = read_all(argv[1]);")
    L.append('    if (!input) { fprintf(stderr, "cannot read %s\\n", argv[1]); return 2; }')
    L.append("    char *err = NULL; json_t *root = json_parse(input, &err);")
    L.append('    if (err) { fprintf(stderr, "parse error: %s\\n", err); free(err); free(input); return 2; }')
    L.append('    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\\n"); free(input); return 2; }')
    L.append("    int n = json_array_size(root);")
    L.append("    for (int i = 0; i < n; i++){")
    L.append("        json_t *c = json_get(root, i);")
    L.append('        const char *op = json_get_str(c, "op", "");')
    L.append("        json_t *o = NULL;")
    for fn, _, _ in funcs:
        L.append('        if (strcmp(op, "%s") == 0) o = emit_%s(c);' % (fn, fn))
    L.append('        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }')
    L.append("        char *ser = json_serialize(o); printf(\"%s\\n\", ser); free(ser); json_free(o);")
    L.append("    }")
    L.append("    json_free(root); free(input); return 0;")
    L.append("}")
    return "\n".join(L) + "\n"


def gen_oracle(mod, funcs):
    # distinct modules to load
    mods = sorted(set(pym for _, pym, _ in funcs))
    L = []
    L.append('"""AUTO-GENERATED integration oracle for %s (gen_integration_oracle.py)."""' % mod)
    L.append("import os, sys, json, importlib.util")
    L.append("")
    L.append("MODS = {}")
    L.append("def _load(rel):")
    L.append("    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))")
    L.append("    if _repo not in sys.path: sys.path.insert(0, _repo)")
    L.append("    for base in sys.path:")
    L.append("        cand = os.path.join(base, rel)")
    L.append("        try:")
    L.append("            spec = importlib.util.spec_from_file_location('live_' + rel.replace('/', '_').replace('.', '_'), cand)")
    L.append("            mod = importlib.util.module_from_spec(spec)")
    L.append("            spec.loader.exec_module(mod)")
    L.append("            return mod")
    L.append("        except Exception:")
    L.append("            continue")
    L.append("    return None")
    for m in mods:
        L.append("MODS[%r] = _load(%r)" % (m, m.replace(".", "/") + ".py"))
    L.append("")
    # DISPATCH maps op (cfunc) -> (pymod, pyfunc)
    L.append("DISPATCH = {")
    for fn, pymod, pyfn in funcs:
        L.append("    %r: (%r, %r)," % (fn, pymod, pyfn))
    L.append("}")
    L.append("")
    L.append("def main():")
    L.append('    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_%s.py <cases.json>\\n"); return 2' % mod)
    L.append("    with open(sys.argv[1], 'r', encoding='utf-8') as f: cases = json.load(f)")
    L.append("    for c in cases:")
    L.append("        op = c.get('op'); value = c.get('value', '')")
    L.append("        d = DISPATCH.get(op)")
    L.append("        if not d: sys.stdout.write(json.dumps({'fn':op}, separators=(',',':')) + '\\n'); continue")
    L.append("        pymod, pyfn = d")
    L.append("        mod = MODS.get(pymod)")
    L.append("        try:")
    L.append("            out = getattr(mod, pyfn)(value) if mod else None")
    L.append("        except Exception as e:")
    L.append("            out = 'PYERR:' + str(e)")
    L.append("        if isinstance(out, bool): out = bool(out)")
    L.append("        elif isinstance(out, (int, float)) and not isinstance(out, bool): out = int(out)")
    L.append("        elif out is None: out = ''")
    L.append("        else: out = str(out)")
    L.append("        sys.stdout.write(json.dumps({'fn':op,'out':out}, ensure_ascii=True, separators=(',',':')) + '\\n')")
    L.append("    return 0")
    L.append("")
    L.append("if __name__ == '__main__':")
    L.append("    sys.exit(main())")
    return "\n".join(L) + "\n"


def gen_fixture(mod, funcs):
    cases = []
    for fn, _, _ in funcs:
        for fx in FIXTURES:
            cases.append({"op": fn, "value": fx})
    return json.dumps(cases, ensure_ascii=False, indent=0)


_CFILE = {}


def main():
    cfile = sys.argv[1]
    mod = os.path.basename(cfile)[:-2]
    _CFILE[mod] = cfile
    funcs = candidate_funcs(cfile)
    sigs = parse_sigs(cfile)
    if not funcs:
        print("NOCAND %s" % mod)
        return
    if "--write" in sys.argv:
        h = gen_harness(mod, cfile, funcs, sigs)
        o = gen_oracle(mod, funcs)
        hpath = os.path.join(SL, "tests", "t_port_%s.c" % mod)
        opath = os.path.join(SL, "tests", "sta_oracle_%s.py" % mod)
        open(hpath, "w").write(h)
        open(opath, "w").write(o)
        fdir = os.path.join(SL, "tests", "oracle", "fixtures", mod)
        os.makedirs(fdir, exist_ok=True)
        open(os.path.join(fdir, "%s.in" % mod), "w").write(gen_fixture(mod, funcs))
        # register in registry.json
        reg = json.load(open(os.path.join(SL, "tests", "oracle", "registry.json")))
        reg["ports"][mod] = {
            "harness": "tests/t_port_%s.c" % mod,
            "oracle": "tests/sta_oracle_%s.py" % mod,
            "fixtures": "tests/oracle/fixtures/%s" % mod,
        }
        json.dump(reg, open(os.path.join(SL, "tests", "oracle", "registry.json"), "w"), indent=2)
        print("WROTE %s :: %d funcs -> %s + %s + fixtures/%s" % (mod, len(funcs), hpath, opath, mod))
    else:
        print("CAND %s :: %d funcs: %s" % (mod, len(funcs), [f[0] for f in funcs]))


if __name__ == "__main__":
    main()
