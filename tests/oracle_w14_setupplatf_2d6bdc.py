#!/usr/bin/env python3
"""Differential oracle for w14_setupplatf_2d6bdc (hermes_cli/setup_platforms.py).

The pure validators are called directly on the live module; `_save_port` gets a
recording hermes_cli.setup double so its persist/warn decision is observable.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
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
    m = sys.modules.get(name)
    if m is None:
        m = _mod(name)
    if not hasattr(m, "__path__"):
        m.__path__ = []
    return m


_pkg("hermes_cli")  # parent-package bootstrap

ACTIONS = []
_mod("hermes_cli.setup",
     save_env_value=lambda k, v: ACTIONS.append(("save", v)),
     print_success=lambda m: None,
     print_warning=lambda m: ACTIONS.append(("warn", m)))

SRC = Q + "/hermes_cli/setup_platforms.py"
spec = importlib.util.spec_from_file_location("hermes_cli.setup_platforms_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def compute(r):
    f = r.get("func")
    if f == "valid_telegram_token":
        return live._is_valid_telegram_bot_token(r.get("token", ""))
    if f == "is_progress":
        return live._is_progress(r.get("status", ""))
    if f == "allowlist":
        return r.get("users", "").replace(" ", "")
    if f == "save_port":
        del ACTIONS[:]
        live._save_port("WEBHOOK_PORT", r.get("value", ""), r.get("default", "8644"))
        if ACTIONS and ACTIONS[0][0] == "save":
            return {"action": "save", "value": ACTIONS[0][1]}
        if ACTIONS and ACTIONS[0][0] == "warn":
            return {"action": "warn", "value": ""}
        return {"action": "none", "value": ""}
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
        print(f"LIVE ERROR {r.get('func')} case={r.get('case')}: {exc!r}",
              file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)

print(f"ORACLE w14_setupplatf_2d6bdc: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
