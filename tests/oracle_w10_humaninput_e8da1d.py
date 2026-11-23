#!/usr/bin/env python3
"""Differential oracle for tools/human_input_hooks.py (gate w10_humaninput_e8da1d)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_humaninput_e8da1d"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    return mod


# Parent-package bootstrap: the live module's _redacted() imports agent.redact
# lazily; quarry is on sys.path so the real redactor executes (no stub).
for _pkg in ("tools", "agent", "gateway", "hermes_cli"):
    try:
        __import__(_pkg)
    except Exception:
        _seed(_pkg)

SRC = QUARRY + "/tools/human_input_hooks.py"
_spec = importlib.util.spec_from_file_location("human_input_hooks_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["human_input_hooks_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "redacted":
        return live._redacted(rec.get("text") or "")
    if func == "resolve_outcome":
        req = live.HumanInputRequest("fixed-id", outcome=rec.get("outcome"))
        return str(req.outcome or "error")
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
        if rec.get("func") not in ("redacted", "resolve_outcome"):
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
