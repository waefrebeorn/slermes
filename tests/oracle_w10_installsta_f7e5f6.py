#!/usr/bin/env python3
"""Differential oracle for hermes_cli/install_state.py (gate w10_installsta_f7e5f6)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_installsta_f7e5f6"
sys.path.insert(0, QUARRY)

try:
    import hermes_cli  # noqa: F401
except Exception:
    pass

SRC = QUARRY + "/hermes_cli/install_state.py"
_spec = importlib.util.spec_from_file_location("install_state_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["install_state_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "unit_account":
        return live._unit_account(rec.get("unit", ""))
    home = types.SimpleNamespace(config=rec.get("config", {}),
                                 auth_stores=tuple(rec.get("auth_stores", [])))
    if func == "onboarding_done":
        return live._onboarding_done(home)
    if func == "provider_config":
        return live._provider_config(home)
    if func == "provider_login":
        return live._provider_login(home)
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
        if rec.get("func") not in ("unit_account", "onboarding_done",
                                    "provider_config", "provider_login"):
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
