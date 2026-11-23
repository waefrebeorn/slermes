#!/usr/bin/env python3
"""Differential oracle for w13_defaultplu_f7fec3 (hermes_cli/default_plugins.py)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/default_plugins.py"
sys.path.insert(0, QUARRY)
import hermes_cli  # noqa: F401  parent-package bootstrap
spec = importlib.util.spec_from_file_location("hermes_cli.default_plugins", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.default_plugins"] = live
spec.loader.exec_module(live)

# Control os.environ and gateway.config_loader.read_yaml_layers per case.
cfg_module = types.ModuleType("gateway.config_loader")
sys.modules["gateway.config_loader"] = cfg_module
_gw = sys.modules.get("gateway")
if _gw is None:
    _gw = types.ModuleType("gateway")
    _gw.__path__ = [QUARRY + "/gateway"]
    sys.modules["gateway"] = _gw
_gw.config_loader = cfg_module


class _Entry:
    def __init__(self, name):
        self.name = name


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False))


def compute(r):
    f = r.get("func")
    if f == "attempt":
        return live._attempt(_Entry(r.get("name", "")))
    env = r.get("env", "")
    sec = r.get("security")
    readable = bool(r.get("readable", True))
    live.os = types.SimpleNamespace(environ={"HERMES_DISABLE_LAZY_INSTALLS": env})

    def read_yaml_layers(home):
        if not readable:
            raise OSError("unreadable")
        return {"security": sec} if sec is not None else {}
    cfg_module.read_yaml_layers = read_yaml_layers
    if f == "fetch_policy":
        return live.fetch_policy("home")
    if f == "fetch_allowed":
        return live.fetch_allowed("home")
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
        py = canon(compute(r))
    except Exception as exc:
        print(f"ORACLE w13_defaultplu_f7fec3: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH w13_defaultplu_f7fec3 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_defaultplu_f7fec3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
