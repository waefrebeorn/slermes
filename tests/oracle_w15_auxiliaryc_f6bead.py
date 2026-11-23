#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_call_outcome.py (gate w15_auxiliaryc_f6bead)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("agent", QUARRY + "/agent")


class _Reason:
    def __init__(self, value):
        self.value = value


class _Classified:
    def __init__(self, value):
        self.reason = _Reason(value)


_STATE = {"reason": "unknown"}
ec = types.ModuleType("agent.error_classifier")
ec.classify_api_error = lambda *a, **kw: _Classified(_STATE["reason"])
sys.modules["agent.error_classifier"] = ec

spec = importlib.util.spec_from_file_location(
    "agent.auxiliary_call_outcome_live", QUARRY + "/agent/auxiliary_call_outcome.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.auxiliary_call_outcome_live"] = live
spec.loader.exec_module(live)

_ERRS = {
    "base": lambda: BaseException("x"),
    "keyboard": lambda: KeyboardInterrupt(),
    "system_exit": lambda: SystemExit(),
    "interrupted": lambda: InterruptedError(),
    "value": lambda: ValueError("x"),
    "runtime": lambda: RuntimeError("x"),
    "exception": lambda: Exception("x"),
    "os_error": lambda: OSError("x"),
}


def _err(kind):
    return _ERRS.get(kind, _ERRS["value"])()


def compute(r):
    f = r.get("func")
    if f == "embedded":
        if "error_object" in r:
            eo = r["error_object"] or {}

            class E:
                pass

            e = E()
            if "message" in eo:
                e.message = eo["message"]
            if "code" in eo:
                e.code = eo["code"]

            class Resp:
                pass

            resp = Resp()
            resp.error = e
            return live.embedded_provider_error(resp)
        return live.embedded_provider_error(r.get("response"))
    if f == "is_cancellation":
        return live.is_cancellation(_err(r.get("kind", "value")))
    if f == "error_class":
        _STATE["reason"] = r.get("stub_reason", "unknown")
        return live.error_class(_err(r.get("kind", "value")),
                                provider=r.get("provider", ""), model=r.get("model", ""))
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
        print("PYERROR %r %r" % (r, exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE w15_auxiliaryc_f6bead: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
