#!/usr/bin/env python3
"""Differential oracle for gateway/session_results.py (gate w9_sessionresu_9c104c).

Loads the LIVE module and drives record_unexecuted_failure / record_terminal_failure
against an explicit execution_result ContextVar value. The C harness returns
{"ret": <unchanged input>, "captured": <mutated dict|null>} and that whole object
is compared with the live result.
Summary: ORACLE w9_sessionresu_9c104c: N cases, M mismatches.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w9_sessionresu_9c104c"
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

SRC = QUARRY + "/gateway/session_results.py"
_spec = importlib.util.spec_from_file_location("session_results_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["session_results_live"] = live
_spec.loader.exec_module(live)


def _stamp_failure(result, reason, retryable):
    result["failure_reason"] = reason
    result["failure_retryable"] = bool(retryable)
    return result


try:
    import agent  # noqa: F401
except Exception:
    _seed("agent")
_seed("agent.turn_failure_copy", stamp_failure=_stamp_failure)


def live_out(rec):
    func = rec.get("func")
    captured = rec.get("captured")
    live.execution_result.set(captured)
    if func == "unexecuted":
        ret = live.record_unexecuted_failure(rec.get("reply"),
                                             reason=rec.get("reason"),
                                             error=rec.get("error"))
    elif func == "terminal":
        ret = live.record_terminal_failure(rec.get("result"))
    else:
        return None
    return {"ret": ret, "captured": live.execution_result.get()}


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
        if rec.get("func") not in ("unexecuted", "terminal"):
            continue
        cases += 1
        py = live_out(rec)
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
