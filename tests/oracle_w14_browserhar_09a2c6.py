#!/usr/bin/env python3
"""Differential oracle for tools/browser_harness_dirs.py (gate w14_browserhar_09a2c6)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_browserhar_09a2c6"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/tools/browser_harness_dirs.py"
_spec = importlib.util.spec_from_file_location("browser_harness_dirs_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["browser_harness_dirs_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    env = rec.get("env") or {}
    if func == "identity":
        return list(live.daemon_identity(env))
    if func == "daemon_env":
        return live.daemon_env(env, tuple(rec.get("identity") or ()))
    if func == "budget":
        return live.socket_dir_budget(rec.get("platform", ""))
    if func == "fits":
        return live.fits_socket_budget(rec.get("resolved_dir", ""), rec.get("budget"))
    if func == "home":
        return live._harness_home(env)
    if func == "pid_file":
        return str(live.harness_pid_file(env, rec.get("suffix", ".pid")))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("identity", "daemon_env", "budget", "fits", "home", "pid_file"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            print(f"LIVE ERROR {rec.get('func')}: {exc!r}", file=sys.stderr)
            mism += 1
            continue
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
