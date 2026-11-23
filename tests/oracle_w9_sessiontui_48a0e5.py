#!/usr/bin/env python3
"""Differential oracle for gateway/session_tui_ops.py (gate w9_sessiontui_48a0e5)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w9_sessiontui_48a0e5"
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
# session_tui_ops imports gateway.session_busy_controls / session_tui_reads at module top
_seed("gateway.session_busy_controls", authorize=lambda *a, **k: None)
_seed("gateway.session_tui_reads", context_breakdown=lambda *a, **k: {})

SRC = QUARRY + "/gateway/session_tui_ops.py"
_spec = importlib.util.spec_from_file_location("session_tui_ops_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["session_tui_ops_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "wire_bytes":
        return live._wire_bytes(rec.get("text"))
    if func == "fit_frame":
        return live._fit_frame(rec.get("text"), int(rec["budget"]))
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
        if rec.get("func") not in ("wire_bytes", "fit_frame"):
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
