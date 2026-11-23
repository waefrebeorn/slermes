#!/usr/bin/env python3
"""Differential oracle for gateway/session_clarify_lock.py (gate clarifylock).

Loads the LIVE clarify_lock via importlib and drives the real async function
with a scripted gateway.session_server_requests module. For `lock`, the scripted
lock_answer() result (or ValueError) is compared with the C harness verdict. For
`coerce_answer`, a recording lock_answer() captures the answer the LIVE function
passes after its `json.dumps(..., ensure_ascii=False)` coercion, and that captured
value is compared with the C harness's coerced answer.

Input (harness output, one JSON per line): a `lock` or `coerce_answer` case.
Summary: ORACLE clarifylock: N cases, M mismatches.
"""
import asyncio
import importlib.util
import json
import sys
import types
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


try:
    import gateway  # noqa: F401
except Exception:
    _seed("gateway")

SRC = QUARRY + "/gateway/session_clarify_lock.py"
_spec = importlib.util.spec_from_file_location("clarifylock_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["clarifylock_live"] = live
_spec.loader.exec_module(live)

AuthorityRefusal = __import__("persistence.core.errors", fromlist=["AuthorityRefusal"]).AuthorityRefusal


def _run(params, lock_answer, may_settle=None):
    _seed("gateway.session_server_requests",
          lock_answer=lock_answer,
          may_settle=may_settle or (lambda *a: True))
    conn = NS(authority=object(), actor=object())
    return asyncio.run(live.clarify_lock(conn, NS(session_id="s1"), params))


def live_lock(rec):
    params = rec.get("params")
    remaining = rec.get("remaining")
    raises = bool(rec.get("lock_raises"))

    def lock_answer(rid, qid, answer, session_ok=None):
        if raises:
            raise ValueError("scripted lock failure")
        return remaining

    try:
        return _run(params, lock_answer)
    except AuthorityRefusal as exc:
        return {"__raise__": "AuthorityRefusal", "msg": str(exc)}


def live_coerce(rec):
    captured = {}

    def lock_answer(rid, qid, answer, session_ok=None):
        captured["answer"] = answer
        return 0

    params = {"request_id": "r", "question_id": "q"}
    if "answer" in rec:
        params["answer"] = rec["answer"]
    _run(params, lock_answer)
    return captured["answer"]


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        func = rec.get("func")
        if func == "lock":
            py = live_lock(rec)
        elif func == "coerce_answer":
            py = live_coerce(rec)
        else:
            continue
        cases += 1
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH clarifylock case={rec.get('case')} func={func} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print("ORACLE clarifylock: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE clarifylock: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
