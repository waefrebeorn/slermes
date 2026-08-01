#!/usr/bin/env python3
"""
gen_oracle.py - best-effort behavioral-oracle generator for slermes ports.

For a ported C module (mapped from a Python module via PoP: annotations),
synthesize a C harness + Python reference oracle + fixture that replay the
Python function's behavior against the C port using type-derived sample inputs.

Targets the mechanically-oracle-able subset: standalone functions whose
signatures use only simple scalar types (int/long/size_t/bool/double/float/char)
and which return a scalar or char* (JSON-serializable). Modules exposing class
methods, struct-heavy APIs, or requiring live network/DB/runtime state are
reported UNSUITED rather than faked.

Usage:  gen_oracle.py <python_module.py> [<c_file.c>] [--write]
"""
import sys, os, re, json

SL = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

SIMPLE_TYPES = {"int", "long", "size_t", "bool", "char", "double", "float",
                "unsigned", "uint", "uint8_t", "uint32_t", "uint64_t",
                "int8_t", "int32_t", "int64_t"}

def cpath_for(py):
    if py.startswith("agent/"):
        return "src/agent/port_" + py[len("agent/"):].replace("/", "_")[:-3] + ".c"
    if py.startswith("tools/"):
        return "src/tools/port_" + py[len("tools/"):].replace("/", "_")[:-3] + ".c"
    if py.startswith("hermes_cli/"):
        return "src/cli/port_" + py[len("hermes_cli/"):].replace("/", "_")[:-3] + ".c"
    if py.startswith("gateway/"):
        return "src/gateway/port_" + py[len("gateway/"):].replace("/", "_")[:-3] + ".c"
    return None

def find_cfile(py):
    c = cpath_for(py)
    if c and os.path.isfile(os.path.join(SL, c)):
        return c
    for root, _, files in os.walk(os.path.join(SL, "src")):
        for f in files:
            if not f.endswith(".c"):
                continue
            p = os.path.join(root, f)
            try:
                txt = open(p, encoding="utf-8", errors="replace").read()
            except Exception:
                continue
            if re.search(r"PoP:\s*\S+\s+@\s+" + re.escape(py) + r"\b", txt):
                return os.path.relpath(p, SL)
    return None

POP_RE = re.compile(r"PoP:\s*(\w+)\s+@\s+([\w/]+\.py)(?::(\w+))?")

def extract_pops(cfile):
    txt = open(os.path.join(SL, cfile), encoding="utf-8", errors="replace").read()
    out = []
    for m in POP_RE.finditer(txt):
        cfunc, pymod, pyfunc = m.group(1), m.group(2), m.group(3) or m.group(1)
        out.append((cfunc, pymod, pyfunc))
    return out

SIG_RE = re.compile(r"([\w]+(?:\s*\*)?)\s+(\w+)\s*\(([^)]*)\)\s*\{", re.S)

_SIG_CACHE = {}
def parse_sig(cfile, func):
    if cfile not in _SIG_CACHE:
        try:
            txt = open(os.path.join(SL, cfile), encoding="utf-8", errors="replace").read()
        except Exception:
            _SIG_CACHE[cfile] = {}
            return None
        d = {}
        for m in SIG_RE.finditer(txt):
            d[m.group(2)] = (m.group(1).strip(), m.group(2).strip())
        _SIG_CACHE[cfile] = d
    return _SIG_CACHE[cfile].get(func)

def classify_params(params):
    if params.strip() in ("void", ""):
        return "simple", []
    parts = [p.strip() for p in params.split(",")]
    types = []
    for p in parts:
        base = p.replace("*", "").strip().split()[-1] if p else ""
        if "*" in p or "struct" in p or base not in SIMPLE_TYPES:
            return "complex", p
        types.append(base)
    return "simple", types

def c_default(t):
    if t in ("int", "long", "size_t", "unsigned", "uint",
             "uint8_t", "uint32_t", "uint64_t", "int8_t", "int32_t", "int64_t"):
        return "1"
    if t == "bool":
        return "1"
    if t in ("double", "float"):
        return "1.5"
    if t == "char":
        return "'x'"
    return '"x"'

def py_default(t):
    if t in ("int", "long", "size_t", "unsigned", "uint",
             "uint8_t", "uint32_t", "uint64_t", "int8_t", "int32_t", "int64_t"):
        return "1"
    if t == "bool":
        return "True"
    if t in ("double", "float"):
        return "1.5"
    if t == "char":
        return "'x'"
    return "'x'"

def is_scalar_ptr(ret):
    # char* return -> treat as string (json serializable); other pointer -> complex
    return ret.strip() == "char*"

def gen_harness(name, cfile, simple):
    base = os.path.basename(cfile)[:-2]
    hdr = cfile[:-2] + ".h"
    inc = (os.path.basename(hdr) if os.path.isfile(os.path.join(SL, hdr))
           else os.path.basename(cfile))
    L = []
    L.append("/* AUTO-GENERATED oracle harness for %s (gen_oracle.py). */" % name)
    L.append('#include <stdio.h>')
    L.append('#include <stdlib.h>')
    L.append('#include <string.h>')
    L.append('#include "%s"' % inc)
    L.append("")
    L.append("static const char *js(const char *s){")
    L.append("  static char b[4][4096]; static int t=0; char *q=b[t]; t=(t+1)&3; *q++='\"';")
    L.append("  if(s) for(const char *p=s;*p&&(q-b[t])<4000;p++){unsigned char c=*p;")
    L.append("    if(c=='\"'||c=='\\\\'){*q++='\\\\';*q++=c;}")
    L.append("    else if(c=='\\n'){*q++='\\\\';*q++='n';}")
    L.append("    else if(c=='\\t'){*q++='\\\\';*q++='t';}")
    L.append("    else if(c<' '){*q++='\\\\';*q++='u';*q++='0';*q++='0';*q++=(c>15?'1':'0');*q++=(c%16<10?c%16+'0':c%16-10+'a');}")
    L.append("    else *q++=c;}")
    L.append("  *q++='\"'; *q=0; return b[t];")
    L.append("}")
    L.append("int main(void){")
    L.append("  setvbuf(stdout, NULL, _IONBF, 0);")
    for cfunc, pymod, pyfunc in simple:
        sig = parse_sig(cfile, cfunc)
        if not sig:
            continue
        ret, params = sig
        _, types = classify_params(params)
        args = ", ".join(c_default(t) for t in types)
        if ret == "void":
            L.append("  %s(%s);" % (cfunc, args))
            continue
        if ret == "char*":
            L.append('  printf("{\\"func\\":\\"%s\\",\\"ret\\":%s}\\n", "%s", js(%s(%s)));'
                     % (cfunc, "%s", cfunc, cfunc, args))
        elif ret == "bool":
            L.append('  printf("{\\"func\\":\\"%s\\",\\"ret\\":%d}\\n", "%s", (int)%s(%s));'
                     % (cfunc, 0, cfunc, cfunc, args))
        elif ret in ("int", "long", "size_t", "unsigned", "uint",
                     "uint8_t", "uint32_t", "uint64_t", "int8_t", "int32_t", "int64_t"):
            L.append('  printf("{\\"func\\":\\"%s\\",\\"ret\\":%d}\\n", "%s", %s(%s));'
                     % (cfunc, 0, cfunc, cfunc, args))
        elif ret in ("double", "float"):
            L.append('  printf("{\\"func\\":\\"%s\\",\\"ret\\":%f}\\n", "%s", %s(%s));'
                     % (cfunc, 0.0, cfunc, cfunc, args))
        elif ret == "char":
            L.append('  printf("{\\"func\\":\\"%s\\",\\"ret\\":\\"%c\\"}\\n", "%s", %s(%s));'
                     % (cfunc, "x", cfunc, cfunc, args))
    L.append("  return 0;")
    L.append("}")
    return "\n".join(L) + "\n"

def gen_oracle(name, pymod, simple):
    mod = pymod[:-3].replace("/", ".")
    pyfuncs = sorted(set(pyf for _, _, pyf in simple))
    L = []
    L.append('"""AUTO-GENERATED oracle for %s (gen_oracle.py)."""' % name)
    L.append("import sys, json, os")
    L.append("sys.path.insert(0, os.path.expanduser(\"~/hermes-agent-dev\"))")
    L.append("from %s import (%s)" % (mod, ", ".join(pyfuncs)))
    L.append("")
    L.append("mism = 0; n = 0")
    L.append("for line in sys.stdin:")
    L.append("    line = line.strip()")
    L.append("    if not line.startswith('{'):")
    L.append("        continue")
    L.append("    rec = json.loads(line)")
    L.append("    n += 1")
    L.append("    fn = rec['func']")
    # per-function python call with matching defaults
    for cfunc, pymod2, pyfunc in simple:
        sig = parse_sig(find_cfile_of(name) or "", cfunc)
        # recompute types from the simple list is hard; instead hardcode via a dispatch dict
    # Build a dispatch table mapping func -> (pyfunc, [defaults])
    L.append("    ARGS = {")
    for cfunc, pymod2, pyfunc in simple:
        sig = parse_sig(_CFILE_CACHE.get(name, ""), cfunc)
        if not sig:
            continue
        _, params = sig
        _, types = classify_params(params)
        defaults = ", ".join(py_default(t) for t in types)
        L.append('        %r: (%r, [%s]),' % (cfunc, pyfunc, defaults))
    L.append("    }")
    L.append("    if fn not in ARGS:")
    L.append("        continue")
    L.append("    pyf, pargs = ARGS[fn]")
    L.append("    try:")
    L.append("        exp = pyf(*pargs)")
    L.append("    except Exception as e:")
    L.append("        print('PYERR', fn, e); continue")
    L.append("    got = rec['ret']")
    L.append("    if isinstance(exp, str): exp = exp")
    L.append("    if got != exp:")
    L.append("        # loose compare for floats / bool/int")
    L.append("        ok = False")
    L.append("        try:")
    L.append("            if abs(float(got) - float(exp)) < 1e-6: ok = True")
    L.append("        except Exception: pass")
    L.append("        if not ok:")
    L.append("            mism += 1")
    L.append("            print('MISMATCH', fn, 'got', got, 'exp', exp)")
    L.append('print("AUTO oracle: %d cases, %d mismatches" % (n, mism))')
    L.append("sys.exit(1 if mism else 0)")
    return "\n".join(L) + "\n"

# helper to find cfile again inside gen_oracle (name -> cfile)
_CFILE_CACHE = {}
def find_cfile_of(name):
    return _CFILE_CACHE.get(name)

def main():
    py = sys.argv[1]
    cfile = sys.argv[2] if len(sys.argv) > 2 else find_cfile(py)
    if not cfile or not os.path.isfile(os.path.join(SL, cfile)):
        print(json.dumps({"name": py, "suitability": "NOCFILE", "reason": "no c file"}))
        return
    pops = extract_pops(cfile)
    if not pops:
        # NOPOP: mirror the c-file path to a python module and check directly
        # (no full src/ tree walk).
        cand = None
        if "/agent/port_" in cfile:
            cand = "agent/" + cfile.split("/agent/port_")[1][:-2] + ".py"
        elif "/tools/port_" in cfile:
            cand = "tools/" + cfile.split("/tools/port_")[1][:-2] + ".py"
        elif "/cli/port_" in cfile:
            cand = "hermes_cli/" + cfile.split("/cli/port_")[1][:-2] + ".py"
        elif "/gateway/port_" in cfile:
            cand = "gateway/" + cfile.split("/gateway/port_")[1][:-2] + ".py"
        if cand and os.path.isfile(os.path.join(SL, cand)):
            txt = open(os.path.join(SL, cfile), encoding="utf-8", errors="replace").read()
            pysrc = open(os.path.join(SL, cand), encoding="utf-8", errors="replace").read()
            pydefs = set(re.findall(r"def\s+(\w+)\s*\(", pysrc))
            simple = []
            for m in SIG_RE.finditer(txt):
                ret, params = m.group(1).strip(), m.group(2).strip()
                if ret == "static" or ret.startswith("static"):
                    continue
                cfunc = m.group(2)
                kind, types = classify_params(params)
                if kind == "simple" and (ret.replace("*","").strip() in SIMPLE_TYPES or ret=="void" or ret=="char*"):
                    pyf = cfunc if cfunc in pydefs else None
                    if not pyf:
                        for pd in pydefs:
                            if pd in cfunc or cfunc in pd or pd.replace("_","")==cfunc.replace("_",""):
                                pyf = pd; break
                    if pyf:
                        simple.append((cfunc, cand, pyf))
            if simple:
                name = cand.replace("/","_")[:-3]
                rep = {"name": name, "cfile": cfile, "suitability": "SIMPLE",
                       "nfuncs": len(simple), "funcs": simple, "via": "NOPOP-name-match"}
                if "--write" in sys.argv:
                    _CFILE_CACHE[name]=cfile
                    h = gen_harness(name, cfile, simple)
                    o = gen_oracle(name, cand, simple)
                    open(os.path.join(SL,"tests","t_port_%s.c"%name),"w").write(h)
                    open(os.path.join(SL,"tests","sta_oracle_%s.py"%name),"w").write(o)
                    fdir=os.path.join(SL,"tests/oracle/fixtures",name)
                    os.makedirs(fdir,exist_ok=True)
                    open(os.path.join(fdir,"cases.in"),"w").write("single\n")
                    rep["harness"]="tests/t_port_%s.c"%name
                    rep["oracle"]="tests/sta_oracle_%s.py"%name
                print(json.dumps(rep)); return
        print(json.dumps({"name": py, "cfile": cfile, "suitability": "NOPOP",
                          "reason": "no PoP annotations and no name-mapped python original"}))
        return
    name = py.replace("/", "_")[:-3]
    _CFILE_CACHE[name] = cfile
    simple = []
    ncomplex = 0
    for cfunc, pymod, pyfunc in pops:
        sig = parse_sig(cfile, cfunc)
        if not sig:
            continue
        ret, params = sig
        kind, types = classify_params(params)
        if kind == "simple" and (ret.replace("*","").strip() in SIMPLE_TYPES or ret=="void" or ret=="char*"):
            simple.append((cfunc, pymod, pyfunc))
        else:
            ncomplex += 1
    if not simple:
        print(json.dumps({"name": name, "cfile": cfile, "suitability": "COMPLEX",
                          "reason": "all functions complex", "ncomplex": ncomplex, "npops": len(pops)}))
        return
    rep = {"name": name, "cfile": cfile, "suitability": "SIMPLE",
           "nfuncs": len(simple), "ncomplex": ncomplex, "funcs": simple}
    if "--write" in sys.argv:
        h = gen_harness(name, cfile, simple)
        o = gen_oracle(name, pops[0][1], simple)
        open(os.path.join(SL, "tests", "t_port_%s.c" % name), "w").write(h)
        open(os.path.join(SL, "tests", "sta_oracle_%s.py" % name), "w").write(o)
        fdir = os.path.join(SL, "tests/oracle/fixtures", name)
        os.makedirs(fdir, exist_ok=True)
        open(os.path.join(fdir, "cases.in"), "w").write("single\n")
        rep["harness"] = "tests/t_port_%s.c" % name
        rep["oracle"] = "tests/sta_oracle_%s.py" % name
    print(json.dumps(rep))

if __name__ == "__main__":
    main()
