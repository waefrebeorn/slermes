#!/usr/bin/env python3
"""Differential oracle for agent/session_client_env.py (gate w10_sessioncli_766251).

Imports the LIVE module with a stubbed tools.code_execution_env secret policy;
the secret substrings are injected per case from the fixture.
"""
import importlib.util
import json
import sys
import types

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

tools = _pkg("tools")
ce = types.ModuleType("tools.code_execution_env")
SECRET = {"v": []}
ce._SECRET_SUBSTRINGS = []
ce.is_secret_env_name = lambda n: any(s in n.upper() for s in SECRET["v"])
tools.code_execution_env = ce
sys.modules["tools.code_execution_env"] = ce

SRC = QUARRY + "/agent/session_client_env.py"
spec = importlib.util.spec_from_file_location("w10_sessioncli_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_sessioncli_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    secret = list(r.get("secret") or [])
    SECRET["v"] = secret
    ce._SECRET_SUBSTRINGS = secret
    if func == "configured_names":
        return sorted(live.configured_names(r.get("config")))
    if func == "client_env_rules":
        return live.client_env_rules(r.get("config"), local=bool(r.get("local", True)))
    if func == "travels":
        return live._travels(r.get("name", ""), secret)
    if func == "normalise":
        try:
            return {"error": False, "env": live.normalise_client_env(r.get("value"), r.get("rules"))}
        except ValueError:
            return {"error": True, "env": None}
    if func == "accept":
        try:
            env = live.accept_client_env(r.get("value"), r.get("config"), local=bool(r.get("local", True)))
            return {"error": False, "env": env}
        except ValueError:
            return {"error": True, "env": None}
    if func == "retry_matches":
        committed = {
            "client_env_json": (json.dumps(r["stored_env"]) if r.get("stored_env") is not None else None),
            "client_env_rules_json": (json.dumps(r["rules"]) if r.get("rules") is not None else None),
        }
        request = {"client_env": r.get("request_env")} if r.get("has_client_env") else {}
        return live.retry_matches(committed, request)
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
    print("ORACLE w10_sessioncli_766251: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w10_sessioncli_766251: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
