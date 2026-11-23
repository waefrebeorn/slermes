#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/bridged_env.py (noc_bridgedenv.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/gateway/bridged_env.py"
sys.path.insert(0, QUARRY)

# Bootstrap the parent package so late relative/absolute imports resolve.
if "gateway" not in sys.modules:
    _pkg = types.ModuleType("gateway")
    _pkg.__path__ = [QUARRY + "/gateway"]
    sys.modules["gateway"] = _pkg

spec = importlib.util.spec_from_file_location("bridgedenv_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["bridgedenv_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "digest":
        return live.bridged_env_digest(r.get("value"))
    if func == "drop_markers":
        return live.drop_bridge_markers(dict(r.get("env") or {}))
    if func == "follow":
        scope = dict(r.get("scope") or {})
        live_env = dict(r.get("live") or {})
        grant = r.get("grant")
        # Explicit seams: inject os.environ and bridged_allow_all_users().
        old_os = live.os
        live.os = types.SimpleNamespace(environ=live_env)
        fake_cl = types.ModuleType("gateway.config_loader")
        fake_cl.bridged_allow_all_users = lambda: grant
        old_cl = sys.modules.get("gateway.config_loader")
        sys.modules["gateway.config_loader"] = fake_cl
        try:
            live.follow_bridged_env(scope)
        finally:
            live.os = old_os
            if old_cl is None:
                sys.modules.pop("gateway.config_loader", None)
            else:
                sys.modules["gateway.config_loader"] = old_cl
        return scope
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
        print(f"ORACLE bridgedenv: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH bridgedenv {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE bridgedenv: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
