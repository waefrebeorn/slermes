#!/usr/bin/env python3
"""Differential oracle for hermes_cli/_old_updater.py (w9_oldupdater_42fb74)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

G = "w9_oldupdater_42fb74"
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc

SRC = Q + "/hermes_cli/_old_updater.py"
spec = importlib.util.spec_from_file_location("oldupdater_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["oldupdater_live"] = live
spec.loader.exec_module(live)


class _Code:
    def __init__(self, name, filename):
        self.co_name = name
        self.co_filename = filename


class _Frame:
    def __init__(self, name, module, filename):
        self.f_code = _Code(name, filename or "")
        self.f_globals = {} if module is None else {"__name__": module}


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "is_historical_frame":
            main_path = r.get("main_py_path")
            # Make the live module's idea of "this checkout's main.py" come
            # from the fixture so both sides compare the same paths.
            if main_path:
                live.__file__ = str(Path(main_path).with_name("_old_updater.py"))
            frame = _Frame(r.get("co_name"), r.get("module"), r.get("co_filename"))
            py = live._is_historical_update_frame(frame)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if bool(py) != bool(got):
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
