#!/usr/bin/env python3
"""Differential oracle for agent/safe_worker_policy.py (gate w10_safeworker_419ccd).

Process-bound state is injected: one-shot flag, presence of runtime imports,
the frozen policy snapshot and the calling pid.  The live module is imported
via importlib after a parent-package bootstrap.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m

_pkg("agent")  # parent-package bootstrap

SRC = QUARRY + "/agent/safe_worker_policy.py"
spec = importlib.util.spec_from_file_location("w10_safeworker_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_safeworker_live"] = live
spec.loader.exec_module(live)


def _err(exc):
    s = str(exc)
    if "booleans" in s:
        return "type_fields"
    if "_bind_worker_config" in s:
        return "value_ordinary"
    if "JSON object" in s:
        return "type_config"
    if "already bound" in s:
        return "runtime_already"
    if "runtime imports" in s:
        return "runtime_imports"
    if "fork" in s:
        return "fork"
    return "other:" + s


def _set_policy(r):
    pol = r.get("policy")
    if pol is None:
        live._policy = None
        return
    live._policy = SimpleNamespace(
        pid=pol.get("pid", 0),
        safe_mode=pol.get("safe_mode", False),
        ignore_user_config=pol.get("ignore_user_config", False),
        config_json=json.dumps(pol.get("config", {})),
    )


def compute(r):
    func = r.get("func")
    # Fresh environment per case: no one-shot binding, no runtime imports.
    live._policy = None
    for name in live._RUNTIME_IMPORTS:
        sys.modules.pop(name, None)
    if r.get("runtime_imports"):
        sys.modules["run_agent"] = types.ModuleType("run_agent")
    live.os = SimpleNamespace(getpid=lambda: r.get("caller_pid", 0))

    try:
        if func == "bind_safe_worker_policy":
            live._policy = (SimpleNamespace(pid=0, safe_mode=True, ignore_user_config=True,
                                            config_json="{}") if r.get("already_bound") else None)
            pol = live._bind_safe_worker_policy(
                safe_mode=r.get("safe_mode"),
                ignore_user_config=r.get("ignore_user_config"),
                config=r.get("config"))
            return {"error": None, "ok": True,
                    "safe_mode": pol.safe_mode, "ignore_user_config": pol.ignore_user_config}
        if func == "bind_worker_config":
            live._policy = (SimpleNamespace(pid=0, safe_mode=False, ignore_user_config=False,
                                            config_json="{}") if r.get("already_bound") else None)
            pol = live._bind_worker_config(r.get("config"))
            return {"error": None, "ok": True,
                    "safe_mode": pol.safe_mode, "ignore_user_config": pol.ignore_user_config}
        if func == "current_policy":
            _set_policy(r)
            return {"error": None, "enabled": live.safe_worker_enabled()}
        if func == "worker_config_snapshot":
            _set_policy(r)
            return live.worker_config_snapshot()
    except Exception as exc:
        if func in ("bind_safe_worker_policy", "bind_worker_config"):
            return {"error": _err(exc), "ok": False, "safe_mode": None, "ignore_user_config": None}
        if func == "current_policy":
            return {"error": _err(exc), "enabled": False}
        raise
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
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w10_safeworker_419ccd: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w10_safeworker_419ccd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
