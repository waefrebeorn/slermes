#!/usr/bin/env python3
"""Ranked automatable fix queue for the slermes C port.

Reads the honest parity scan (with recursive bootleg detection) and produces a
prioritized, *automatable* queue of bootleg PoP functions. Each entry is
classified by how it can be closed:

  A  DELEGATE   : a real, non-bootleg C function already exists (by base-name
                  match to a live handler in cli_cmd_*.c / commands.c / a
                  real ported fn). Just rewire the call.
  B  PORT_DIRECT: pure/print/config/string/file-IO logic — port the Python
                  body directly, low risk.
  C  DEEP_PORT  : needs a genuinely unported subsystem (OAuth flow, TUI launch,
                  electron build, update orchestration). High effort.

We also score by module-gap concentration so a session can drain the highest-
value cluster first. Output: tests/ranked_fix_queue.json (+ a .md summary).
"""
import json, os, re, sys

SLERMES = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/s3.json"

# Real (non-bootleg) C function names: everything defined outside port_*.c/port_*.h
REAL_FUNCS = set()
SRC = os.path.join(SLERMES, "src")
for dp, _, fs in os.walk(SRC):
    for f in fs:
        if not f.endswith(".c") or f.startswith("port_"):
            continue
        try:
            txt = open(os.path.join(dp, f), encoding="utf-8", errors="ignore").read()
        except Exception:
            continue
        for m in re.finditer(
            r'(?:static\s+)?(?:const\s+)?(?:unsigned\s+)?'
            r'(?:void|int|bool|char\s*\*|json_t\s*\*|size_t|long|double|float|'
            r'unsigned\s+int|const\s+char\s*\*)\s+(\*?\w+)\s*\(',
            txt):
            REAL_FUNCS.add(m.group(1))


def base_of(name):
    n = name
    for p in ("main_u_", "main_", "auth_u_", "auth_", "hermes_cli_", "ccm_",
             "cli_", "port_", "cmd_"):
        if n.startswith(p):
            n = n[len(p):]
            break
    return n


def classify(py_name, c_func, c_file, py_body=None, ported_py_names=None):
    b = base_of(c_func)
    pb = base_of(py_name)
    # A: delegatable — a real C fn matches the base name
    candidates = {b, pb, "cmd_" + pb, "cmd_" + b}
    for r in REAL_FUNCS:
        rb = base_of(r)
        if rb and (rb == b or rb == pb) and r not in (
                "main", "if", "for", "while", "cmd"):
            return "A", sorted(candidates & REAL_FUNCS)
    # B: name heuristics for direct-portable pure/IO helpers
    low = c_func.lower()
    direct = any(k in low for k in (
        "print", "format", "label", "version", "notice", "hint", "status",
        "parse", "detect", "is_", "has_", "check", "path", "read", "write",
        "get_", "set_", "default", "str", "to_", "from_", "encode", "decode",
        "hash", "sanitize", "validate", "resolve", "build", "make", "render",
        "size", "count", "len", "name", "title", "desc", "color", "quote",
    ))
    if direct:
        # Refine B into Bt (trivial: only safe primitives, no calls to other
        # unported helpers) vs Bx (needs dependency ports). Trivial ones are
        # safe to port directly; Bx must wait for their deps.
        if py_body is not None and is_trivial_body(py_body,
                ported_py_names or set()):
            return "Bt", []
        return "Bx", []
    return "C", []


# --- trivial-body detection via AST -----------------------------------------
import ast

_SAFE_NODES = (ast.Expression, ast.Call, ast.Name, ast.Load, ast.Store,
               ast.Constant, ast.Attribute, ast.Subscript, ast.Index,
               ast.Slice, ast.BinOp, ast.UnaryOp, ast.BoolOp, ast.Compare,
               ast.IfExp, ast.If, ast.Return, ast.Assign, ast.AugAssign,
               ast.Str, ast.Num, ast.List, ast.Tuple, ast.Dict, ast.Set,
               ast.comprehension, ast.ListComp, ast.DictComp, ast.SetComp,
               ast.GeneratorExp, ast.For, ast.While, ast.Break, ast.Continue,
               ast.Pass, ast.Str, ast.JoinedStr, ast.FormattedValue,
               ast.Try, ast.ExceptHandler, ast.With, ast.Starred, ast.keyword,
               ast.Lambda, ast.AnnAssign, ast.Expr)
# Known-safe stdlib/str methods that map to trivial C helpers.
_SAFE_CALLS = {
    "len", "str", "int", "float", "bool", "sorted", "reversed", "enumerate",
    "range", "min", "max", "abs", "format", "isinstance", "getattr",
    "print", "open", "strip", "lstrip", "rstrip", "lower", "upper", "split",
    "join", "replace", "startswith", "endswith", "find", "rfind", "count",
    "partition", "rpartition", "zfill", "center", "ljust", "rjust",
}


def is_trivial_body(src, ported_py_names):
    """True iff the function body only uses safe primitives and does NOT call
    any name that is an *unported, non-stdlib* project helper. Allowed:
      - safe builtins / str methods (_SAFE_CALLS)
      - calls to another bootleg PoP fn (port the cluster; matches with or
        without a leading underscore)
      - methods on known stdlib modules (os, re, sys, json, math, time,
        pathlib, shutil, subprocess, datetime, collections, itertools,
        functools, hashlib, base64, urllib)
    Conservative: unknown/dynamic targets => not trivial."""
    _STDLIB_MODULES = {"os", "re", "sys", "json", "math", "time", "pathlib",
                       "shutil", "subprocess", "datetime", "collections",
                       "itertools", "functools", "hashlib", "base64", "urllib",
                       "argparse", "logging", "textwrap", "string", "typing"}
    try:
        tree = ast.parse(src)
    except Exception:
        return False
    for node in ast.walk(tree):
        if isinstance(node, ast.Call):
            f = node.func
            if isinstance(f, ast.Name):
                name = f.id
                if name in _SAFE_CALLS:
                    continue
                if name in ported_py_names or ("_" + name) in ported_py_names:
                    continue
                return False
            elif isinstance(f, ast.Attribute):
                base = f.value
                # walk to the root of the attribute chain (os.path.basename -> os)
                while isinstance(base, ast.Attribute):
                    base = base.value
                if isinstance(base, ast.Name) and base.id in _STDLIB_MODULES:
                    continue  # os.path.basename(...), re.match(...), etc.
                if f.attr in _SAFE_CALLS:
                    continue
                return False
            else:
                return False  # dynamic/unknown call target
    return True


# Build a py-name -> source map for triviality checks.
_PY_SRC_CACHE = {}


def load_py_bodies():
    if _PY_SRC_CACHE:
        return
    root = os.path.dirname(SLERMES)  # /home/wubu/hermes-agent-dev
    pyroot = os.path.join(root, "hermes_cli")
    # scan both hermes_cli and tools (most bootleg modules live there)
    for base in (pyroot, os.path.join(root, "tools"),
                 os.path.join(root, "agent"), os.path.join(root, "gateway")):
        if not os.path.isdir(base):
            continue
        for dp, _, fs in os.walk(base):
            for f in fs:
                if not f.endswith(".py"):
                    continue
                try:
                    txt = open(os.path.join(dp, f), encoding="utf-8",
                              errors="ignore").read()
                except Exception:
                    continue
                try:
                    tree = ast.parse(txt)
                except Exception:
                    continue
                for n in ast.walk(tree):
                    if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)):
                        _PY_SRC_CACHE[n.name] = ast.get_source_segment(txt, n)
    return


def main():
    load_py_bodies()
    d = json.load(open(SCAN))
    mods = d.get("modules", {})
    # all bootleg Python function names (the ported set we can cluster-port)
    ported_py_names = set()
    for mod, info in mods.items():
        for g in info.get("gaps", []):
            if g.get("classification") == "REAL_GAP":
                pf = g.get("python_feature", {})
                if pf.get("name"):
                    ported_py_names.add(pf["name"])
    queue = []
    for mod, info in mods.items():
        for g in info.get("gaps", []):
            if g.get("classification") != "REAL_GAP":
                continue
            pf = g.get("python_feature", {})
            py_name = pf.get("name", "")
            c_func = g.get("c_function") or ""
            c_file = g.get("c_location", "")
            if not c_func:
                continue
            cat, cand = classify(py_name, c_func, c_file,
                                 _PY_SRC_CACHE.get(py_name), ported_py_names)
            queue.append({
                "module": mod,
                "py": py_name,
                "c": c_func,
                "file": c_file,
                "cat": cat,
                "candidates": cand,
            })
    # rank: A first, then Bt, then Bx, then C
    mod_gaps = {}
    for q in queue:
        mod_gaps[q["module"]] = mod_gaps.get(q["module"], 0) + 1
    cat_rank = {"A": 0, "Bt": 1, "Bx": 2, "C": 3}
    queue.sort(key=lambda q: (cat_rank[q["cat"]],
                              -mod_gaps[q["module"]],
                              q["module"], q["py"]))
    out = {
        "total": len(queue),
        "by_cat": {k: sum(1 for q in queue if q["cat"] == k)
                   for k in ("A", "Bt", "Bx", "C")},
        "by_module": mod_gaps,
        "queue": queue,
    }
    jp = os.path.join(SLERMES, "tests", "ranked_fix_queue.json")
    json.dump(out, open(jp, "w"), indent=1)
    md = []
    md.append(f"# Ranked fix queue — {len(queue)} bootleg functions\n")
    md.append(f"- A  (delegate to real fn): {out['by_cat']['A']}")
    md.append(f"- Bt (trivial direct-port): {out['by_cat']['Bt']}")
    md.append(f"- Bx (direct but needs deps): {out['by_cat']['Bx']}")
    md.append(f"- C  (deep subsystem):        {out['by_cat']['C']}\n")
    md.append("## Top of queue (automatable first)\n")
    for q in queue[:200]:
        cand = ",".join(q["candidates"][:4]) if q["candidates"] else ""
        md.append(f"- [{q['cat']}] {q['module']} :: {q['py']} -> {q['c']}"
                  + (f"  (cands: {cand})" if cand else ""))
    open(os.path.join(SLERMES, "tests", "ranked_fix_queue.md"), "w").write(
        "\n".join(md))
    print(f"total={len(queue)} A={out['by_cat']['A']} Bt={out['by_cat']['Bt']} "
          f"Bx={out['by_cat']['Bx']} C={out['by_cat']['C']}")
    print("wrote tests/ranked_fix_queue.json + .md")


if __name__ == "__main__":
    main()
