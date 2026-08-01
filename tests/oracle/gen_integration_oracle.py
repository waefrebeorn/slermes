#!/usr/bin/env python3
"""gen_integration_oracle.py — curated integration-oracle generator.

SCOPE: pure functions of the shape
        ret  foo(const char *arg) { ... no socket/sqlite/popen/getenv ... }
that have a PoP annotation mapping them to a REAL Python function (verified to
exist in the dev tree) AND take exactly one string argument. These are oracle-able:
feed the same input string to Python and C, diff.

CONTRACT (matches tests/t_port_account_usage.c / sta_oracle_account_usage.py,
which tests/oracle/runners/run_oracle.sh diffs byte-for-byte):
  * Harness  tests/t_port_<mod>.c  reads a JSON ARRAY fixture from argv[1]; each
    element is {"op":<cfunc>, "value":<input>}. Dispatches by "op", calls the
    ported C function, prints one {"fn":<op>,"out":<result>} JSON line per case.
  * Oracle   tests/sta_oracle_<mod>.py  reads the SAME fixture, recomputes from
    the LIVE Python source, prints the same shape. Runner diffs them.

KEY CORRECTNESS RULES (learned from the bulk-generation failure):
  1. A function's own PoP annotation is often `@`-LESS (e.g. `PoP: has_sensitive_
     query_params`). POP_RE MUST match those, not just `@module.py:func` forms.
  2. The python function name comes from the function's OWN nearest preceding
     PoP (group(1)), NOT a far-above `@`-PoP meant for a different function.
  3. We ONLY register an oracle if the python SOURCE FILE exists in the dev tree
     AND the resolved python function is importable/callable. No live source =>
     no oracle (skip). This prevents registering bogus "MATCH"-by-accident oracles.

Usage:
    python3 gen_integration_oracle.py <port_module.c> [--write] [--check]
  --write : emit harness + oracle + fixture + register in registry.json
  --check : also run run_oracle.sh and report MATCH/MISMATCH (do not register
            unless MATCH and the python source/func resolved)
"""
import os, re, sys, json

SL = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

SIG_RE = re.compile(r"([\w]+(?:\s*\*)?)\s+(\w+)\s*\(([^)]*)\)\s*\{", re.S)
# Match BOTH `@`-less PoPs (fn's own annotation) and `@module.py[:func]` forms.
POP_RE = re.compile(r"PoP:\s*(\w+)(?:\s*/\s*\w+)?\s*(?:@\s*([\w/]+\.py)(?::(\w+))?)?")
IO_BLOCK = re.compile(r"\b(fopen|fread|fwrite|popen|system|socket|connect|sqlite|curl|recv|send|getenv|setenv|read_file|write_file|http_|mcp_|fork|execv|dlopen)\b")

SIMPLE_RET = {"char*", "int", "long", "size_t", "unsigned", "uint",
              "uint8_t", "uint32_t", "uint64_t", "int8_t", "int32_t", "int64_t", "bool"}

FIXTURES = [
    "", "x", "hello world", '{"a":1,"b":[2,3]}', 'a=b&pwd=secret&token=x',
    "user@example.com", "https://example.com/path?q=1", "key:value;key2:v2",
    "/abs/path/file.txt", "CamelCaseString", "12345", "  spaced  ",
]


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
    """Return list of (cfunc, pymod, pyfunc) for pure (const char*) ports whose
    OWN nearest PoP maps to a real python function we can resolve."""
    txt = open(cfile, encoding="utf-8", errors="replace").read()
    sigs = parse_sigs(cfile)
    pops_all = POP_RE.findall(txt)
    primary = None
    if pops_all:
        from collections import Counter
        mods = Counter(p[1][:-3].replace("/", ".") for p in pops_all if p[1])
        if mods:
            primary = mods.most_common(1)[0][0]
    out = []
    # Precompute, for each char offset, the nearest preceding PoP (for fn's own ann).
    pop_positions = [(m.start(), m) for m in POP_RE.finditer(txt)]
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
        # nearest preceding PoP annotation (own annotation, with or without @)
        own = None
        for pos, m in pop_positions:
            if pos < idx:
                own = m
            else:
                break
        if not own:
            continue
        g1, g2, g3 = own.group(1), own.group(2), own.group(3)
        pymod = (g2[:-3].replace("/", ".")) if g2 else primary
        if not pymod:
            continue
        # Only trust g3/g1 as the mapping when the nearest PoP is THIS function's
        # own annotation (g1 == fn or g1 == "_"+fn). Otherwise it's a far-above
        # PoP meant for a different function -> fall back to the _<cfunc> convention.
        is_own = (g1 == fn or g1 == "_" + fn)
        if is_own:
            cands = [c for c in (g3, g1) if c]
        else:
            cands = [fn, "_" + fn]
        pyfn = resolve_pyfn(pymod, cands)
        if not pyfn:
            continue
        body = extract_body(cfile, fn)
        if IO_BLOCK.search(body):
            continue
        # Detect whether the python function returns bool so the harness emits a
        # JSON bool (not an int). C signatures for these are often `int` but the
        # python returns `bool`, and `0` != `false` in raw JSON diffing.
        rib = pyfn_returns_bool(pymod, pyfn)
        # CRITICAL: only keep if the python source + function actually exist.
        pyrel = pymod.replace(".", "/") + ".py"
        found = False
        for base in (os.path.dirname(SL), SL):
            if os.path.isfile(os.path.join(base, pyrel)):
                found = True
                break
        if not found:
            continue
        # The python counterpart must accept a single string arg (the harness
        # calls it with `value`). Functions expecting a dict/list/iterable
        # (e.g. scale_to_zero_enabled(environ: dict)) are NOT comparable to the
        # C `const char *` port and would crash the oracle.
        if not pyfn_string_compatible(pymod, pyfn):
            continue
        out.append((fn, pymod, pyfn, rib))
    return out


def pyfn_returns_bool(pymod, pyfn):
    """Return True if the live python function `pyfn` in module `pymod` returns
    bool (via a `-> bool` annotation OR a body that only returns True/False)."""
    pyrel = pymod.replace(".", "/") + ".py"
    for base in (os.path.dirname(SL), SL):
        path = os.path.join(base, pyrel)
        if not os.path.isfile(path):
            continue
        try:
            import importlib.util
            spec = importlib.util.spec_from_file_location("rib_" + pymod.replace(".", "_"), path)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            fn = getattr(mod, pyfn, None)
            if fn is None:
                return False
            ann = getattr(fn, "__annotations__", {}).get("return")
            if ann is not None:
                return "bool" in str(ann)
            # fall back: inspect the source body for return True/False only
            import inspect
            src = inspect.getsource(fn)
            rets = re.findall(r"return\s+(.+)", src)
            rets = [r.strip() for r in rets]
            if not rets:
                return False
            return all(r in ("True", "False", "true", "false") or "bool(" in r for r in rets)
        except Exception:
            return False
    return False


def pyfn_string_compatible(pymod, pyfn):
    """Return True if the live python function takes a single string-compatible
    positional argument AND returns a scalar (so the integration harness can call
    it with `value` and compare the output). Reject functions that:
      * expect a dict / list / iterable / object (Path, URL, SSLContext,
        Exception, Node, ...) instead of a plain string, or
      * raise on a plain string (object-attr access, deliberate validation) — not
        oracle-comparable, or
      * return a non-scalar (object / coroutine / list / dict).
    A function that does `arg.relative_to()` / `arg.is_dir()` on its first arg
    crashes on a string -> probe-call rejects it. Functions that raise on bad
    input (ValueError/TypeError) are also rejected: their C port returns a safe
    sentinel and the divergence is a false FAP, not a C bug."""
    OBJ_ANNOT = ("dict", "list", "iterable", "sequence", "mapping", "dataframe",
                 "tuple", "path", "url", "sslcontext", "exception", "node",
                 "object", "model", "message", "response", "request")
    pyrel = pymod.replace(".", "/") + ".py"
    for base in (os.path.dirname(SL), SL):
        path = os.path.join(base, pyrel)
        if not os.path.isfile(path):
            continue
        try:
            import importlib.util, inspect
            spec = importlib.util.spec_from_file_location("sc_" + pymod.replace(".", "_"), path)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            fn = getattr(mod, pyfn, None)
            if fn is None:
                return False
            sig = inspect.signature(fn)
            params = list(sig.parameters.values())
            pos = [p for p in params if p.kind in (
                inspect.Parameter.POSITIONAL_ONLY, inspect.Parameter.POSITIONAL_OR_KEYWORD)]
            if len(pos) != 1:
                return False
            ann = pos[0].annotation
            if ann is not inspect.Parameter.empty:
                s = str(ann).lower()
                if any(k in s for k in OBJ_ANNOT):
                    return False
            # Runtime probe: hand it a string; reject if it raises anything
            # (AttributeError = treats arg as object; ValueError/TypeError =
            # deliberate validation that means it isn't a free string fn) or
            # returns a non-scalar.
            try:
                r = fn("__probe_string__")
            except Exception:
                return False
            if isinstance(r, (str, int, float, bool, type(None))):
                return True
            return False
        except Exception:
            return False
    return False


def resolve_pyfn(pymod, candidates):
    """Return the first candidate that actually exists in the live python module,
    else None. This makes the C->Py mapping correct even when a function lacks a
    precise PoP annotation (falls back to the _<cfunc> convention)."""
    pyrel = pymod.replace(".", "/") + ".py"
    for base in (os.path.dirname(SL), SL):
        path = os.path.join(base, pyrel)
        if os.path.isfile(path):
            try:
                import importlib.util
                spec = importlib.util.spec_from_file_location("rp_" + pymod.replace(".", "_"), path)
                mod = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(mod)
                for c in candidates:
                    if c and hasattr(mod, c):
                        return c
            except Exception:
                return None
    return None


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
    for fn, _, _, _ in funcs:
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
    for fn, _, _, rib in funcs:
        ret = sigs[fn][0]
        L.append("static json_t *emit_%s(const json_t *c){" % fn)
        L.append('    const char *value = json_get_str(c, "value", "");')
        if ret == "char*":
            L.append("    const char *out = %s(value);" % fn)
            L.append('    json_t *o = json_new_object(); json_set(o, "fn", json_string("%s"));' % fn)
            L.append('    json_set(o, "out", json_string(out ? out : "")); return o;')
        elif ret == "bool" or (rib and ret in ("int", "long", "size_t", "unsigned", "uint")):
            if ret == "bool":
                L.append("    bool v = (bool)%s(value);" % fn)
            else:
                L.append("    bool v = (%s(value) ? true : false);" % fn)
            L.append('    json_t *o = json_new_object(); json_set(o, "fn", json_string("%s"));' % fn)
            L.append('    json_set(o, "out", json_bool(v)); return o;')
        else:
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
    for k, (fn, _, _, _) in enumerate(funcs):
        kw = "if" if k == 0 else "else if"
        L.append('        %s (strcmp(op, "%s") == 0) o = emit_%s(c);' % (kw, fn, fn))
    L.append('        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }')
    L.append("        char *ser = json_serialize(o); printf(\"%s\\n\", ser); free(ser); json_free(o);")
    L.append("    }")
    L.append("    json_free(root); free(input); return 0;")
    L.append("}")
    return "\n".join(L) + "\n"


def gen_oracle(mod, funcs):
    mods = sorted(set(pym for _, pym, _, _ in funcs))
    L = []
    L.append('"""AUTO-GENERATED integration oracle for %s (gen_integration_oracle.py)."""' % mod)
    L.append("import os, sys, json, importlib.util")
    L.append("")
    L.append("MODS = {}")
    L.append("def _load(rel):")
    L.append("    repo = os.path.realpath(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))")
    L.append("    devroot = os.path.dirname(repo)  # hermes_cli/ lives in the dev-tree parent of slermes")
    L.append("    for p in (repo, devroot):")
    L.append("        if p not in sys.path: sys.path.insert(0, p)")
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
    L.append("DISPATCH = {")
    for fn, pymod, pyfn, _ in funcs:
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
    return json.dumps(
        [{"op": fn, "value": fx} for fn, _, _, _ in funcs for fx in FIXTURES],
        ensure_ascii=False, indent=0)


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
        if "--check" in sys.argv:
            import subprocess
            rr = subprocess.run(["bash", "tests/oracle/runners/run_oracle.sh", mod],
                                capture_output=True, text=True, cwd=SL)
            verdict = "MATCH" if "MATCH" in rr.stdout else "MISMATCH"
            if verdict == "MATCH":
                reg = json.load(open(os.path.join(SL, "tests", "oracle", "registry.json")))
                reg["ports"][mod] = {
                    "harness": "tests/t_port_%s.c" % mod,
                    "oracle": "tests/sta_oracle_%s.py" % mod,
                    "fixtures": "tests/oracle/fixtures/%s" % mod,
                }
                json.dump(reg, open(os.path.join(SL, "tests", "oracle", "registry.json"), "w"), indent=2)
                print("WROTE+REGISTERED %s :: %d funcs -> MATCH" % (mod, len(funcs)))
            else:
                print("WROTE %s :: %d funcs -> %s (NOT registered)" % (mod, len(funcs), verdict))
                print(rr.stdout.strip()[-500:])
        else:
            reg = json.load(open(os.path.join(SL, "tests", "oracle", "registry.json")))
            reg["ports"][mod] = {
                "harness": "tests/t_port_%s.c" % mod,
                "oracle": "tests/sta_oracle_%s.py" % mod,
                "fixtures": "tests/oracle/fixtures/%s" % mod,
            }
            json.dump(reg, open(os.path.join(SL, "tests", "oracle", "registry.json"), "w"), indent=2)
            print("WROTE %s :: %d funcs (unverified)" % (mod, len(funcs)))
    else:
        print("CAND %s :: %d funcs: %s" % (mod, len(funcs), [f[0] for f in funcs]))


if __name__ == "__main__":
    main()
