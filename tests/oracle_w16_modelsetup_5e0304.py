#!/usr/bin/env python3
"""Differential oracle for hermes_cli/model_setup_flows.py (gate w16_modelsetup_5e0304)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_modelsetup_5e0304"
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

_MODS = [
    "hermes_cli.config",
    "hermes_cli.model_setup_flows_custom",
    "hermes_cli.model_setup_flows_azure",
    "hermes_cli.model_setup_flows_bedrock",
    "hermes_cli.main_provider_setup",
]
for _name in _MODS:
    _m = _Stub(_name)
    _m.__path__ = []
    sys.modules[_name] = _m
    setattr(_hc, _name.rsplit(".", 1)[1], _m)

_common = _Stub("hermes_cli.model_setup_flows_common")
_common.__path__ = []
_common._HTTP = ("http://", "https://")
sys.modules["hermes_cli.model_setup_flows_common"] = _common
_hc.model_setup_flows_common = _common

_auth = _Stub("hermes_cli.auth")
_auth.__path__ = []
_auth.ZAI_ENDPOINTS = [
    ("global", "https://api.z.ai/api/paas/v4", ["glm-5"], "Global"),
    ("cn", "https://open.bigmodel.cn/api/paas/v4", ["glm-5"], "China"),
    ("coding-global", "https://api.z.ai/api/coding/paas/v4", ["glm-5"], "Global (Coding Plan)"),
    ("coding-cn", "https://open.bigmodel.cn/api/coding/paas/v4", ["glm-5"], "China (Coding Plan)"),
]
sys.modules["hermes_cli.auth"] = _auth
_hc.auth = _auth

_SELECTED = [None]
_OVERRIDE = [""]
sys.modules["hermes_cli.main_provider_setup"]._prompt_provider_choice = (
    lambda choices, default=None, title=None: _SELECTED[0])

_spec = importlib.util.spec_from_file_location(
    "hermes_cli.model_setup_flows_live", QUARRY + "/hermes_cli/model_setup_flows.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)
live._ask = lambda *a, **k: _OVERRIDE[0]
live._HTTP = ("http://", "https://")


def _compute(r):
    f = r.get("func")
    if f == "nous_args":
        args = types.SimpleNamespace(**(r.get("args") or {}))
        return vars(live._nous_login_args(args))
    if f == "zai_endpoint":
        sel = r.get("selected")
        if isinstance(sel, float) and sel.is_integer():
            sel = int(sel)
        _SELECTED[0] = sel
        _OVERRIDE[0] = r.get("override", "")
        return live._select_zai_endpoint(r.get("current", ""))
    return None


def _canon(x):
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
