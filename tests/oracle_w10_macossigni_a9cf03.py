#!/usr/bin/env python3
"""Differential oracle for hermes_cli/macos_signing.py (gate w10_macossigni_a9cf03)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_macossigni_a9cf03"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/hermes_cli/macos_signing.py"
_spec = importlib.util.spec_from_file_location("macos_signing_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["macos_signing_live"] = live
_spec.loader.exec_module(live)


class _Completed:
    def __init__(self, rc):
        self.returncode = rc
        self.stderr = ""
        self.stdout = ""


def live_out(rec):
    calls = {"n": 0}

    def fake_run(args, **kw):
        i = calls["n"]
        calls["n"] += 1
        if rec.get("sign_raises") and i == 0:
            raise RuntimeError("codesign exploded")
        return _Completed(rec.get("sign_rc", 0) if i == 0 else rec.get("verify_rc", 0))

    live.platform = types.SimpleNamespace(system=lambda: rec.get("system", "Windows"))
    live.shutil = types.SimpleNamespace(
        which=lambda name: "/usr/bin/codesign" if rec.get("codesign_available") else None)
    live.subprocess = types.SimpleNamespace(run=fake_run)
    return live.sign_managed_python(Path("/opt/hermes/python/bin/python3"))


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
        if rec.get("func") != "sign_managed_python":
            continue
        cases += 1
        py = live_out(rec)
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
