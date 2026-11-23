#!/usr/bin/env python3
"""Differential oracle for pm/plugin_inputs.py (gate w6_plugininput_ee7719).

Both pure functions run LIVE.  decode()'s frozen dataclass result is normalised
back to a JSON descriptor; encode()'s Path.absolute() cwd is injected by
temporarily patching os.getcwd (no process environment mutation).
"""
import importlib.util, json, os, sys, types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

PKG = "w6_plugininput_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/pm"]
sys.modules[PKG] = pkg
spec = importlib.util.spec_from_file_location(PKG + ".plugin_inputs", Q + "/pm/plugin_inputs.py")
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".plugin_inputs"] = live
spec.loader.exec_module(live)


def make_obj(spec):
    if spec is None or spec.get("type") == "none":
        return None
    t = spec["type"]
    if t == "members":
        if "sources" in spec:
            return live.Members({Path(k): Path(v) for k, v in spec["sources"]})
        return live.Members([Path(d) for d in spec.get("dirs", [])])
    if t == "candidates":
        return live.Candidates([Path(d) for d in spec.get("dirs", [])])
    if t == "selection":
        return live.Selection(spec.get("data", {}))
    if t == "staged":
        return live.StagedUpdate(spec.get("data", {}))
    raise ValueError("bad spec")


def norm(obj):
    if obj is None:
        return None
    t = type(obj).__name__
    if t == "Members":
        if isinstance(obj.dirs, dict):
            return {"type": "members",
                    "sources": [[str(k), str(v)] for k, v in obj.dirs.items()]}
        return {"type": "members", "dirs": [str(p) for p in obj.dirs]}
    if t == "Candidates":
        return {"type": "candidates", "dirs": [str(p) for p in obj.dirs]}
    if t == "Selection":
        return {"type": "selection", "data": obj.data}
    if t == "StagedUpdate":
        return {"type": "staged", "data": obj.data}
    return None


def compute(r):
    func = r.get("func")
    if func == "decode":
        try:
            return norm(live.decode(r.get("value")))
        except ValueError as exc:
            return str(exc)
    if func == "encode":
        obj = make_obj(r.get("spec"))
        cwd = r.get("cwd", "")
        real = os.getcwd
        os.getcwd = lambda: cwd
        try:
            return live.encode(obj)
        finally:
            os.getcwd = real
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_plugininput_ee7719: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
