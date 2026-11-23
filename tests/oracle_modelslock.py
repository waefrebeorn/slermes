#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/models_cache_lock.py."""
import importlib.util
import json
import sys
import types
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/models_cache_lock.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

spec = importlib.util.spec_from_file_location("modelslock_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["modelslock_live"] = live
spec.loader.exec_module(live)

_EXC = {
    "write_oserror": OSError, "write_timeouterror": TimeoutError,
    "write_valueerror": ValueError,
    "lock_oserror": OSError, "lock_timeouterror": TimeoutError,
    "lock_valueerror": ValueError,
}


def compute(r):
    func = r.get("func")
    if func == "lock_spec":
        p = Path(r["path"])
        return {"lock_path": str(p.with_name(p.name + ".lock")),
                "timeout": 10.0,
                "message": f"Timed out waiting for the provider models cache lock ({p})"}
    if func == "write":
        kind = r.get("raise", "none")
        called, logged = [False], []
        @contextmanager
        def fake_lock(path):
            if kind.startswith("lock_") and kind in _EXC:
                raise _EXC[kind]("boom")
            yield
        def write():
            called[0] = True
            if kind.startswith("write_") and kind in _EXC:
                raise _EXC[kind]("boom")
        old_lock, old_logger = live.provider_models_cache_lock, live.logger
        live.provider_models_cache_lock = fake_lock
        live.logger = types.SimpleNamespace(debug=lambda *a, **k: logged.append(1))
        try:
            try:
                live.write_under_cache_lock(Path(r.get("path", "cache.json")), write)
                outcome = "written"
            except Exception:
                outcome = "propagated"
            if logged:
                outcome = "skipped"
            return {"write_called": called[0], "outcome": outcome}
        finally:
            live.provider_models_cache_lock = old_lock
            live.logger = old_logger
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
        print(f"ORACLE modelslock: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH modelslock {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE modelslock: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
