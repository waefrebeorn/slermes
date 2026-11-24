#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_discovery.py (gate w16_pluginsdis_42aa3c)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_pluginsdis_42aa3c"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


class _Stub(types.ModuleType):
    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        fn = lambda *a, **k: None
        setattr(self, name, fn)
        return fn


_hc = types.ModuleType("hermes_cli")
_hc.__path__ = [QUARRY + "/hermes_cli"]
sys.modules["hermes_cli"] = _hc

for _name in ("hermes_constants", "hermes_cli.config", "hermes_cli.plugin_capabilities",
              "hermes_cli.relay_plugin_cutover"):
    _m = _Stub(_name)
    _m.__path__ = []
    sys.modules[_name] = _m

_pm = _Stub("hermes_cli.plugins_manifest")
_pm.__path__ = []
_pm.manifest_key = lambda m: m.key or m.name
sys.modules["hermes_cli.plugins_manifest"] = _pm

_spec = importlib.util.spec_from_file_location(
    "hermes_cli.plugins_discovery_live", QUARRY + "/hermes_cli/plugins_discovery.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)


def _compute(r):
    f = r.get("func")
    if f == "skipped":
        return live.is_skipped_plugin_dir(r.get("name", ""))
    if f == "ep_group":
        eps = r.get("eps")
        if isinstance(eps, list):
            eps = [types.SimpleNamespace(**e) for e in eps]
        return live._select_entry_point_group(eps, r.get("group", ""))
    if f == "winners":
        manifests = [types.SimpleNamespace(**m) for m in (r.get("manifests") or [])]
        return live.resolve_manifest_winners(manifests)
    return None


def _canon(x):
    if isinstance(x, types.SimpleNamespace):
        return _canon(vars(x))
    if isinstance(x, (list, tuple)):
        return [_canon(v) for v in x]
    if isinstance(x, dict):
        return {k: _canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line or "{" not in line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = _compute(r)
    got = r.get("out")
    if _canon(py) != _canon(got):
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
sys.exit(1 if mism or not cases else 0)
