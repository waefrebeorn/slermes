#!/usr/bin/env python3
"""Oracle for plugins/platforms/slack/adapter_config_readers.py.

The two factories return getters that close over (key, env_var, flag) and
forward them to a `self` reader. We inject a self that renders the forwarded
call as f"{key}|{env_var}|{flag}", and compare name + call result to C.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ROOT = "C:/Users/eman5/hermes-agent"
SRC = ROOT + "/plugins/platforms/slack/adapter_config_readers.py"
spec = importlib.util.spec_from_file_location("slackcfg_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


class FakeSelf:
    def _extra_or_env_flag(self, key, env_var, *, strip=False):
        return f"{key}|{env_var}|{strip}"

    def _extra_or_env_channel_set(self, key, env_var, *, coerce_scalar=False):
        return f"{key}|{env_var}|{coerce_scalar}"


SELF = FakeSelf()

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
    key = r.get("key", "")
    env = r.get("env_var", "")
    if func == "flag_name":
        g = live._extra_or_env_flag_getter(key, env, strip=bool(r.get("strip", False)))
        py = g.__name__
    elif func == "flag_call":
        g = live._extra_or_env_flag_getter(key, env, strip=bool(r.get("strip", False)))
        py = g(SELF)
    elif func == "channel_name":
        g = live._extra_or_env_channel_set_getter(
            key, env, coerce_scalar=bool(r.get("coerce_scalar", False)))
        py = g.__name__
    elif func == "channel_call":
        g = live._extra_or_env_channel_set_getter(
            key, env, coerce_scalar=bool(r.get("coerce_scalar", False)))
        py = g(SELF)
    else:
        continue

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE slackcfg: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
