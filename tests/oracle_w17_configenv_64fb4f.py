#!/usr/bin/env python3
"""Differential oracle for gate w17_configenv_64fb4f (hermes_cli/config_env_routing.py)."""
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


hcli = _pkg("hermes_cli")
cfg = _mod("hermes_cli.config", OPTIONAL_ENV_VARS=set(), _EXTRA_ENV_KEYS=set())


def _setup_hidden(name):
    return name in cfg._HIDDEN_FOR_TEST


cfg._HIDDEN_FOR_TEST = set()
_mod("hermes_cli.setup_hidden_env", is_setup_hidden_env=_setup_hidden)

NAME = "w17cen_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/hermes_cli/config_env_routing.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w17_configenv_64fb4f"


def compute(r):
    inp = r.get("in") or {}
    reg = inp.get("registry") or {}
    cfg.OPTIONAL_ENV_VARS = set(reg.get("optional") or [])
    cfg._EXTRA_ENV_KEYS = set(reg.get("extra") or [])
    cfg._HIDDEN_FOR_TEST = set(reg.get("hidden") or [])
    func = r.get("func")
    if func == "is_registered_env_name":
        return bool(live.is_registered_env_name(inp.get("name", "")))
    if func == "is_env_setting_key":
        return bool(live.is_env_setting_key(inp.get("key", "")))
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
