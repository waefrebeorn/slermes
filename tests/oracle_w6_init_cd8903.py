#!/usr/bin/env python3
"""Differential oracle for providers/__init__.py (gate w6_init_cd8903).

The registry package needs two import-time stubs (hermes_cli.plugins and
providers.base) so the pure helpers can be loaded from the live source.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _pkg(name, path=None):
    m = types.ModuleType(name)
    m.__path__ = [path] if path else []
    sys.modules[name] = m
    return m


_pkg("hermes_cli", QUARRY + "/hermes_cli")
_p = types.ModuleType("hermes_cli.plugins")
_p.get_bundled_plugins_dir = lambda: __import__("pathlib").Path(".")
sys.modules["hermes_cli.plugins"] = _p
_pkg("providers", QUARRY + "/providers")
_b = types.ModuleType("providers.base")
_b.ProviderProfile = object
sys.modules["providers.base"] = _b

spec = importlib.util.spec_from_file_location("providers", QUARRY + "/providers/__init__.py")
live = importlib.util.module_from_spec(spec)
sys.modules["providers"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "host_claims":
        return live._host_claims(r.get("host", ""), r.get("claimed", ""))
    if func == "module_name":
        name = types.SimpleNamespace(name=r.get("name", ""))
        return live._user_module_name(name, r.get("home_key", ""))
    if func == "stamps_settled":
        f = r.get("facts", {})
        live.time = types.SimpleNamespace(time_ns=lambda: int(f.get("now", "0")))
        stamps = tuple(None if s is None else int(s) for s in f.get("stamps", []))
        return live._plugin_dir_stamps_settled(stamps)
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
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_init_cd8903: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
