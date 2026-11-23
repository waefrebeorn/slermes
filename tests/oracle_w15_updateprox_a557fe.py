#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_proxy_env.py (w15_updateprox_a557fe).

The live module's two environment dependencies are injected: dotenv_published_values
is stubbed from each case's `published` map (case-insensitive key match), and
no_proxy_entries uses the exact live regex split. The live functions mutate an env
dict passed from the fixture; the C harness mutates a copy of the same dict.
"""
import importlib.util
import json
import re
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
    m.__path__ = []
    sys.modules[name] = m
    return m


_pkg("hermes_cli")
_pkg("agent")

PUB = {}


def _published(name):
    return frozenset(v for k, vals in PUB.items() if k.upper() == name.upper() for v in vals)


el = types.ModuleType("hermes_cli.env_loader")
el.dotenv_published_values = _published
sys.modules["hermes_cli.env_loader"] = el


def _no_proxy_entries(value=None):
    return [p for p in re.split(r"[\s,]+", (value or "").strip()) if p]


pb = types.ModuleType("agent.proxy_bypass")
pb.no_proxy_entries = _no_proxy_entries
sys.modules["agent.proxy_bypass"] = pb

SRC = Q + "/hermes_cli/update_proxy_env.py"
spec = importlib.util.spec_from_file_location("w15_updateprox_a557fe_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func not in ("entries", "restore", "prefer"):
        continue
    cases += 1
    PUB = dict(r.get("published", {}) or {})
    env = dict(r.get("env", {}) or {})
    try:
        if func == "entries":
            py = _no_proxy_entries(r.get("value", ""))
        else:
            if func == "restore":
                live.restore_user_proxy_env(env)
            else:
                live.prefer_dotenv_proxy_env(env)
            py = env
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w15_updateprox_a557fe: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
