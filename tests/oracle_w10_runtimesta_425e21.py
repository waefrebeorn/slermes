#!/usr/bin/env python3
"""Differential oracle for hermes_cli/runtime_state.py (gate w10_runtimesta_425e21).

Pure decision/path helpers only; lock/base64/filesystem effects are out of scope.
"""
import importlib.util, json, sys, types
from pathlib import PurePosixPath as PP

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
PKG = "w10_runtimesta_live_pkg"
_pkg = types.ModuleType(PKG); _pkg.__path__ = [Q + "/hermes_cli"]; sys.modules[PKG] = _pkg
spec = importlib.util.spec_from_file_location(PKG + ".runtime_state", Q + "/hermes_cli/runtime_state.py")
live = importlib.util.module_from_spec(spec); sys.modules[spec.name] = live; spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "committed":
        row = r.get("row", {})
        return bool(row.get("committed")) or r.get("facts_digest", "") != row["facts_before"]
    if f == "plugin_paths_valid":
        h, t, b, m = (PP(r.get(k, "")) for k in ("home", "target", "backup", "metadata"))
        return bool(not (not t.is_relative_to(h) or t.parent.name != "plugins"
                         or b.parent != t.parent or not b.name.startswith(".previous-")
                         or m != t.parent / ".install-metadata.json"))
    if f == "config_path_valid":
        c, h = PP(r.get("config", "")), PP(r.get("home", ""))
        return bool(c.name == "config.yaml" and c.is_relative_to(h))
    if f == "path_relative_to":
        return PP(r.get("path", "")).is_relative_to(PP(r.get("parent", "")))
    if f == "path_leaf":
        return PP(r.get("path", "")).name
    if f == "path_parent":
        return str(PP(r.get("path", "")).parent)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)
print("ORACLE w10_runtimesta_425e21: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
