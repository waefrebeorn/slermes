#!/usr/bin/env python3
"""Differential oracle for pm/environments_adopt.py (w8_environments_29900b gate).

Filesystem/sys.path state is injected: os.pathsep (for _replace), the resolved
(running, selected) pair (for restart_needed), and the pending reason value.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/pm/environments_adopt.py"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


pmpkg = _mod("pm")
pmpkg.__path__ = [QUARRY + "/pm"]
_mod("pm.environments",
     install_state_dir=lambda root: Path(root) / "install",
     selected_venv=lambda root: Path(root) / "venv",
     site_packages=lambda env: Path(env) / "site-packages",
     venv_bin_dir=lambda env: Path(env) / "bin")

spec = importlib.util.spec_from_file_location("environmentsadopt_w8_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["environmentsadopt_w8_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r["func"]
    if f == "replace":
        sep = r.get("sep", ";")
        oldsep = live.os.pathsep
        live.os.pathsep = sep
        try:
            return live._replace(r.get("value", ""), Path(r.get("old", "")), Path(r.get("new", ""))).replace("\\", "/")
        finally:
            live.os.pathsep = oldsep
    if f == "restart":
        running, selected = r.get("running", ""), r.get("selected", "")
        live._running_and_selected = lambda root: (Path(running), Path(selected))
        return live.restart_needed("unused")
    if f == "note":
        live.note_restart_pending(r.get("value"))
        return live.restart_pending()
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE environmentsadopt: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH environmentsadopt {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE environmentsadopt: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
