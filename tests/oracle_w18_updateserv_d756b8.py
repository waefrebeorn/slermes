#!/usr/bin/env python3
"""Oracle for gate w18_updateserv_d756b8 (hermes_cli/update_serve_resume.py).

Loads the live module with its heavy imports stubbed, then drives main() with
an in-memory context/result and captures the candidates handed to
_respawn_dashboard_processes. sys.executable is injected from the fixture.
"""
import importlib.util
import io
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

pm = types.ModuleType("pm")
pm.__path__ = []
pm_env = types.ModuleType("pm.environments")
pm_env.activate_dependencies = lambda root: None
sys.modules["pm"] = pm
sys.modules["pm.environments"] = pm_env

dp = types.ModuleType("hermes_cli.dashboard_procs")
dp._filter_dashboard_respawn_candidates = lambda candidates, own_home=None: candidates
sys.modules["hermes_cli.dashboard_procs"] = dp

CAPTURED = []
md = types.ModuleType("hermes_cli.main_dashboard")
def _respawn(commands):
    CAPTURED.extend(commands)
    return []
md._respawn_dashboard_processes = _respawn
sys.modules["hermes_cli.main_dashboard"] = md

SRC = QUARRY + "/hermes_cli/update_serve_resume.py"
spec = importlib.util.spec_from_file_location("w18_updateserv_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w18_updateserv_live"] = live
spec.loader.exec_module(live)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        if r.get("func") != "candidates":
            continue
        pending = bool(r.get("pending", True))
        request = {
            "root": r.get("root", ""),
            "home": r.get("home"),
            "stopped_serves": {"pending": pending, "entries": r.get("entries") or []},
        }
        context = SimpleNamespace(read_text=lambda **k: json.dumps(request))
        result = SimpleNamespace(write_text=lambda *a, **k: None)
        live.sys = SimpleNamespace(
            executable=r.get("executable", ""),
            path=SimpleNamespace(insert=lambda *a: None),
            stderr=io.StringIO(),
        )
        CAPTURED.clear()
        try:
            live.main(context, result)
        except Exception as exc:
            print(f"LIVE ERROR args={r}: {exc!r}", file=sys.stderr)
            continue
        py = [{"pid": pid, "command": list(cmd), "hermes_home": hh}
              for (pid, cmd, hh) in CAPTURED]
        cases += 1
        got = r.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH args={r} py={py!r} c={got!r}", file=sys.stderr)
    print(f"ORACLE w18_updateserv_d756b8: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
