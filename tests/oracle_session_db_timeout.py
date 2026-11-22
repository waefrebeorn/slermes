#!/usr/bin/env python3
"""Oracle for cron/scheduler_script.py:_get_session_db_timeout composed with
_timeout_from_env_or_config (env string first, then cron config, then 10.0)."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/cron/scheduler_script.py"
ns = {"float": float, "str": str, "object": object, "Callable": object, "Any": object}
ns["logger"] = type("L", (), {"warning": staticmethod(lambda *a, **k: None),
                              "debug": staticmethod(lambda *a, **k: None)})()
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name in ("_timeout_from_env_or_config", "_get_session_db_timeout"):
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_get_session_db_timeout"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    env, cfg = r.get("env"), r.get("config")
    # Stub the env reader: Python cron_env_setting returns a string (or "").
    env_str = "" if env is None else (str(env) if not isinstance(env, str) else env)
    ns["cron_env_setting"] = lambda _k, _v=env_str: _v
    # Stub config loader: cron.<key> resolved to cfg value.
    ns["_sched"] = type("S", (), {"load_config": staticmethod(
        lambda _v=cfg: ({"cron": {"session_db_timeout_seconds": _v}} if _v is not None else {}))})()
    try:
        py = float(live())
    except Exception as e:
        print(f"SKIP {r}: {e}", file=sys.stderr)
        continue
    c = float(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE session_db_timeout: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
