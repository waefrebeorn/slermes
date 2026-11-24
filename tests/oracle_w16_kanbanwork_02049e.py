#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_worker_env.py (gate w16_kanbanwork_02049e)."""
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
G = "w16_kanbanwork_02049e"
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


# The module's two origin-resident dependencies are injected seams.
# strip_launch_profile_env is identity here (a fresh dict), and the deployment
# names come from the fixture so the allowlist logic itself is what is compared.
_STATE = {"names": []}


def _deployment_env_names():
    return frozenset(_STATE["names"])


def _strip_identity(env, target_home=None):
    return dict(env)


_seed("hermes_cli")
_seed("hermes_cli.config_backend", deployment_env_names=_deployment_env_names)
_seed("tools")
_seed("tools.environments")
_seed("tools.environments.local", strip_launch_profile_env=_strip_identity)

SRC = QUARRY + "/hermes_cli/kanban_worker_env.py"
_spec = importlib.util.spec_from_file_location("kanban_worker_env_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["kanban_worker_env_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "env":
        _STATE["names"] = rec.get("deployment_names") or []
        return live.worker_client_env(rec.get("env") or {}, rec.get("assignee_home", "/home/assignee"))
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
        if rec.get("func") not in ("env",):
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
