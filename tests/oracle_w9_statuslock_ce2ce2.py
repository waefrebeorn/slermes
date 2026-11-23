#!/usr/bin/env python3
"""Differential oracle for gateway/status_lock_dir.py (gate w9_statuslock_ce2ce2)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w9_statuslock_ce2ce2"
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

SRC = QUARRY + "/gateway/status_lock_dir.py"
_spec = importlib.util.spec_from_file_location("status_lock_dir_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["status_lock_dir_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "reset":
        live.reset_unusable_lock_dir_reports(); return None
    if func == "reported":
        return live.unusable_lock_dir_reported(Path(rec["lock_dir"]))
    if func == "recovered":
        live.unusable_lock_dir_recovered(Path(rec["lock_dir"])); return None
    if func == "unusable":
        exc = live.unusable_lock_dir(
            Path(rec["lock_dir"]),
            PermissionError(int(rec["errno"]), rec["strerror"], rec["filename"]))
        return {"errno": exc.errno, "message": exc.strerror,
                "filename": exc.filename, "type": type(exc).__name__}
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
        if rec.get("func") not in ("reset", "reported", "recovered", "unusable"):
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
