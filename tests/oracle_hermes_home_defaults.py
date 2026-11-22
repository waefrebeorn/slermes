#!/usr/bin/env python3
# coverage-targets: _platform_default_hermes_home, _expand_hermes_home, _get_platform_default_hermes_home
"""LIVE oracle for hermes_constants.py:_platform_default_hermes_home."""
import ast
import json
import sys
import types
import os
import posixpath
import ntpath
from types import SimpleNamespace

SRC = "C:/Users/eman5/hermes-agent/hermes_constants.py"
t = ast.parse(open(SRC, encoding="utf-8").read())
fns = [n for n in t.body if isinstance(n, ast.FunctionDef)
       and n.name in ("_platform_default_hermes_home", "_expand_hermes_home", "_get_platform_default_hermes_home")]
for fn in fns: fn.decorator_list = []

class P:
    def __init__(self, p): self.p = p.rstrip("/") if p != "/" else "/"
    def __truediv__(self, other): return P(self.p + "/" + str(other).lstrip("/"))
    def __str__(self): return self.p
P.home = staticmethod(lambda: P(os.environ.get("HOME", "")))

mod = types.ModuleType("sys")
cases = mism = 0
for line in sys.stdin:
    if '"out"' not in line:
        continue
    r = json.loads(line)
    mod.platform = "win32" if r.get("win32") else "linux"
    osp = ntpath if r.get("win32") else posixpath
    fake_os = SimpleNamespace(environ=os.environ, path=osp)
    ns = {"sys": mod, "Path": P, "os": fake_os}
    exec(compile(ast.Module(body=fns, type_ignores=[]), SRC, "exec"), ns)
    py = str(ns["_platform_default_hermes_home"](
        {"HERMES_DATA_DIR_SUFFIX": r.get("suffix", ""),
         "LOCALAPPDATA": r.get("local_appdata", "")},
        lambda: P(r.get("user_home") or "")))
    got = r["out"]
    for k in ("HOME", "MYVAR"):
        os.environ.pop(k, None)
    if r.get("home"):
        os.environ["HOME"] = r["home"]
        os.environ["USERPROFILE"] = r["home"]
    if r.get("myvar"): os.environ["MYVAR"] = r["myvar"]
    py_default = None
    if r.get("win32"):
        mod.platform = "win32"
        os.environ["HERMES_DATA_DIR_SUFFIX"] = r.get("suffix", "")
        os.environ["LOCALAPPDATA"] = r.get("local_appdata", "")
        py_default = str(ns["_get_platform_default_hermes_home"]())
    got_default = r.get("default_out")
    mod.platform = "win32" if r.get("win32") else "linux"
    py_exp = str(ns["_expand_hermes_home"](r.get("expand") or ""))
    got_exp = r.get("expanded")
    cases += 1
    if py_default is not None and py_default != got_default:
        mism += 1
        print(f"MISMATCH default in={r} py={py_default!r} c={got_default!r}", file=sys.stderr)
    if py_exp != got_exp:
        mism += 1
        print(f"MISMATCH expand in={r} py={py_exp!r} c={got_exp!r}", file=sys.stderr)
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE hermes_home_defaults: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
