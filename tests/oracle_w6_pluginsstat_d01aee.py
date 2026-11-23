#!/usr/bin/env python3
"""Differential oracle for pm/plugins_state.py (gate w6_pluginsstat_d01aee).

Both helpers run LIVE.  The manifest read inside plugin_spellings is a filesystem
effect, so it is replaced with an injected `manifest_name` seam via monkeypatch;
the rest of the function (set construction, spellings, sorting) runs real.
"""
import importlib.util, json, sys, types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

PKG = "w6_pluginsstat_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/pm"]
sys.modules[PKG] = pkg
spec = importlib.util.spec_from_file_location(PKG + ".plugins_state", Q + "/pm/plugins_state.py")
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".plugins_state"] = live
spec.loader.exec_module(live)

import pm.plugin_declarations as pd
_real_nmf, _real_rm = pd.native_manifest_file, pd.read_native_manifest


def compute(r):
    func = r.get("func")
    if func == "enabled":
        return live._enabled_from_config(r.get("config", {}))
    if func == "spellings":
        manifest = r.get("manifest_name") or None
        if manifest is not None:
            pd.native_manifest_file = lambda d: "injected-manifest"
            pd.read_native_manifest = lambda f: {"name": manifest}
        else:
            pd.native_manifest_file = lambda d: None
            pd.read_native_manifest = _real_rm
        try:
            res = live.plugin_spellings(Path(r.get("dir_name", "")), r.get("key") or None)
            return sorted(res)
        finally:
            pd.native_manifest_file, pd.read_native_manifest = _real_nmf, _real_rm
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
print(f"ORACLE w6_pluginsstat_d01aee: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
