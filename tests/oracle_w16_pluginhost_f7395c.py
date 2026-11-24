#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_host_child.py (gate w16_pluginhost_f7395c)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_pluginhost_f7395c"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_hc = types.ModuleType("hermes_cli")
_hc.__path__ = [QUARRY + "/hermes_cli"]
sys.modules["hermes_cli"] = _hc

_spec = importlib.util.spec_from_file_location(
    "hermes_cli.plugin_host_child_live", QUARRY + "/hermes_cli/plugin_host_child.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

_Opaque = live.Opaque

# The pure create_client check imports providers.base at call time.
_providers = types.ModuleType("providers")
_providers.__path__ = []
sys.modules["providers"] = _providers
_pbase = types.ModuleType("providers.base")


class _ProviderProfile:
    def create_client(self):
        return None


_pbase.ProviderProfile = _ProviderProfile
sys.modules["providers.base"] = _pbase
_providers.base = _pbase


class _Plain(_ProviderProfile):
    pass


class _Override(_ProviderProfile):
    def create_client(self):
        return "live-sdk-client"


def _compute(r):
    f = r.get("func")
    if f == "decode":
        return live.decode(r.get("value"))
    if f == "host_unsupported":
        profile = _Override() if r.get("overrides") else _Plain()
        reason = live._host_unsupported_reason(profile)
        return reason
    return None


def _canon(x):
    if isinstance(x, bytes):
        return x.decode("latin-1")
    if isinstance(x, _Opaque):
        return {"__opaque__": x.type_name, "repr": x.text}
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
