#!/usr/bin/env python3
"""Differential oracle for tools/approval_human_wait.py (gate w12_approvalhu_7fc5ff)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_approvalhu_7fc5ff"
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


for _pkg in ("tools", "agent"):
    try:
        __import__(_pkg)
    except Exception:
        _seed(_pkg)

CTX = {"timeout": 300.0}
_seed("tools.approval_context", _get_approval_timeout=lambda: CTX["timeout"])
_deadline = _seed("agent.deadline", MAX_SAFE_TIMEOUT_S=86400.0)

SRC = QUARRY + "/tools/approval_human_wait.py"
_spec = importlib.util.spec_from_file_location("approval_human_wait_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["approval_human_wait_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "clamped":
        return live._clamped_window_seconds(
            float(rec["started"]), float(rec["now"]), float(rec["ceiling"]))
    if func == "ceiling":
        CTX["timeout"] = rec["timeout"]
        _deadline.MAX_SAFE_TIMEOUT_S = rec["max_safe"]
        return live.human_wait_ceiling()
    return None


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
        if rec.get("func") not in ("clamped", "ceiling"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
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
