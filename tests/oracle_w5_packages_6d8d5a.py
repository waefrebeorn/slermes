#!/usr/bin/env python3
"""Oracle for the pm/packages.py C port (gate w5_packages_6d8d5a).

AST-extracts the LIVE artifact-URL builder methods and npm_env plus their
module constants (_RUST_TRIPLE, _NODE_PLAT) from pm/packages.py, and MUSL_TARGETS
from pm/store.py. Each method is called with an explicit self (only
Chromium.store_entry reads one attribute). No heavy pm import chain is run.
"""
import ast
import json
import os
import sys
import types
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "pm" / "packages.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "pm" / "packages.py")
STORE_SRC = str(QUARRY / "pm" / "store.py")
sys.path.insert(0, str(QUARRY))

pm = sys.modules.get("pm")
if pm is None:
    pm = types.ModuleType("pm")
    pm.__path__ = [str(QUARRY / "pm")]
    sys.modules["pm"] = pm

ns = {"__name__": "w5_packages_live", "__builtins__": __builtins__, "os": os,
      "frozenset": frozenset}
tree = ast.parse(Path(SRC).read_text(encoding="utf-8"))
for n in tree.body:
    if isinstance(n, ast.Assign):
        tg = n.targets[0]
        if isinstance(tg, ast.Name) and tg.id in ("_RUST_TRIPLE", "_NODE_PLAT"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)

# collect wanted methods (class-scoped) and module functions, exec at module level
WANTED_METHODS = {
    ("Dmgbuild", "fetch_url"), ("Nodejs", "fetch_url"), ("Git", "fetch_url"),
    ("Gh", "fetch_url"), ("Uv", "fetch_url"), ("Ripgrep", "fetch_url"),
    ("Chromium", "store_entry"),
}
FNS = {}
for cls in tree.body:
    if isinstance(cls, ast.ClassDef):
        for m in cls.body:
            if isinstance(m, ast.FunctionDef) and (cls.name, m.name) in WANTED_METHODS:
                code = compile(ast.Module(body=[m], type_ignores=[]), SRC, "exec")
                exec(code, ns)
                FNS[cls.name + "." + m.name] = ns[m.name]
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name == "npm_env":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        FNS["npm_env"] = ns["npm_env"]

# MUSL_TARGETS from the live store module (no import)
store_tree = ast.parse(Path(STORE_SRC).read_text(encoding="utf-8"))
for n in store_tree.body:
    if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) and n.targets[0].id == "MUSL_TARGETS":
        exec(compile(ast.Module(body=[n], type_ignores=[]), STORE_SRC, "exec"), ns)
FNS["npm_env"].__globals__["MUSL_TARGETS"] = ns["MUSL_TARGETS"]
for k in list(FNS):
    FNS[k].__globals__["MUSL_TARGETS"] = ns["MUSL_TARGETS"]

CHROMIUM_SELF = types.SimpleNamespace(name="chromium")


def drive(rec):
    f = rec.get("func")
    v = rec.get("version", "")
    t = rec.get("target", "")
    if f == "w5_packages_6d8d5a_dmgbuild_fetch_url":
        return FNS["Dmgbuild.fetch_url"](None, v, t)
    if f == "w5_packages_6d8d5a_nodejs_fetch_url":
        return FNS["Nodejs.fetch_url"](None, v, t)
    if f == "w5_packages_6d8d5a_git_fetch_url":
        return FNS["Git.fetch_url"](None, v, t)
    if f == "w5_packages_6d8d5a_gh_fetch_url":
        return FNS["Gh.fetch_url"](None, v, t)
    if f == "w5_packages_6d8d5a_uv_fetch_url":
        return FNS["Uv.fetch_url"](None, v, t)
    if f == "w5_packages_6d8d5a_ripgrep_fetch_url":
        return FNS["Ripgrep.fetch_url"](None, v, t)
    if f == "w5_packages_6d8d5a_chromium_store_entry":
        return FNS["Chromium.store_entry"](CHROMIUM_SELF, v, t)
    if f == "w5_packages_6d8d5a_npm_env":
        return FNS["npm_env"](rec.get("cache_dir", ""), rec.get("base_env") or {})
    return {"__unknown__": f}


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        got = rec.get("out")
        try:
            exp = drive(rec)
        except BaseException as e:
            exp = {"__raise__": type(e).__name__, "msg": str(e)}
        n += 1
        if got != exp:
            mism += 1
            if shown < 10:
                shown += 1
                print("MISMATCH %s case %d:\n  C=%s\n  P=%s" % (
                    rec.get("func"), n, json.dumps(got), json.dumps(exp)), file=sys.stderr)
    print("ORACLE w5_packages_6d8d5a: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
