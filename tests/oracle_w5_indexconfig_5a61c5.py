#!/usr/bin/env python3
"""Differential oracle for pm/index_config.py (noc_w5_indexconfig_5a61c5.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/pm/index_config.py"
sys.path.insert(0, QUARRY)

if "pm" not in sys.modules:
    _pkg = types.ModuleType("pm")
    _pkg.__path__ = [QUARRY + "/pm"]
    sys.modules["pm"] = _pkg

spec = importlib.util.spec_from_file_location("indexconfig_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["indexconfig_live"] = live
spec.loader.exec_module(live)

_orig_pip_conf = live.pip_conf_index_url
_orig_read = live.read_npmrc
_orig_user = live.user_npmrc


def simple_env(env_registry, npmrc_registry):
    """Patch read_npmrc/user_npmrc so live.npm_registry* is deterministic."""
    live.user_npmrc = lambda e: "/fake/npmrc"
    live.read_npmrc = lambda path, e: {"registry": npmrc_registry}
    return {"npm_config_registry": env_registry}


def compute(r):
    f = r["func"]
    if f == "is_forwarded":
        return live.is_forwarded(r.get("key", ""))
    if f == "is_index_redirect":
        return live.is_index_redirect(r.get("key", ""))
    if f == "ini_unsafe":
        return live._ini_unsafe(r.get("raw", ""))
    if f == "npmrc_env_replace":
        return live._npmrc_env_replace(r.get("text", ""), dict(r.get("env") or {}))
    if f == "npm_registry":
        env = simple_env(r.get("env_registry", ""), r.get("npmrc_registry", ""))
        return live.npm_registry(env)
    if f == "npm_registry_url":
        env = simple_env(r.get("env_registry", ""), r.get("npmrc_registry", ""))
        return live.npm_registry_url(r.get("url", ""), env)
    if f == "bridged_index_settings":
        live.pip_conf_index_url = lambda e: r.get("pip_conf", "")
        return live.bridged_index_settings(dict(r.get("ambient") or {}))
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
        print(f"ORACLE indexconfig: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH indexconfig {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
    live.pip_conf_index_url = _orig_pip_conf
    live.read_npmrc = _orig_read
    live.user_npmrc = _orig_user
print(f"ORACLE indexconfig: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
