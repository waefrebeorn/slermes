#!/usr/bin/env python3
"""Differential oracle for gate w18_commanddis_8a156e (tui_gateway/command_discovery.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = [Q + "/" + name.replace(".", "/")]
    sys.modules[name] = m
    return m


_pkg("tui_gateway")

NAME = "w18_commanddis_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/tui_gateway/command_discovery.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w18_commanddis_8a156e"


def compute(r):
    func = r.get("func")
    inp = r.get("in") or {}
    if func == "item":
        if "display" in inp:
            return live._item(inp.get("text", ""), inp.get("meta", ""), inp["display"])
        return live._item(inp.get("text", ""), inp.get("meta", ""))
    if func == "details_root_meta":
        return live._details_root_meta(inp.get("candidate", ""))
    if func == "details_completions":
        return live._details_completions(inp.get("text", ""))
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
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
