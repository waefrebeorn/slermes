#!/usr/bin/env python3
'''Differential oracle for gate w14_localcreat_5396fc (session_rules/local_create.py).'''
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_localcreat_5396fc"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/session_rules/local_create.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    f = r.get("func")
    if f == "is_secret": return live.is_secret_env_name(r.get("name"))
    if f == "travels": return live._travels(r.get("name"), r.get("secret"))
    if f == "kept": return live._kept(r.get("name"), r.get("rules"))
    if f == "normalise":
        try:
            return live.normalise_client_env(r.get("value"), r.get("rules"))
        except ValueError as exc:
            return {"error": str(exc)}
    if f == "client_request": return live.client_request(r.get("request_json"), r.get("derived"))
    if f == "same_create_request": return live.same_create_request(r.get("saved_json"), r.get("retry_json"))
    if f == "retry_matches": return live.retry_matches(r.get("committed"), r.get("request"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
