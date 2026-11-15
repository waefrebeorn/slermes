#!/usr/bin/env python3
"""Oracle for pm/environments.py — dependency-environment path resolution.

Python's str(Path) uses backslashes on Windows; the C port joins with "/", so
both sides are normalised before comparison.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from hermes_constants import (                    # noqa: E402
    reset_hermes_home_override, set_hermes_home_override,
)
from pm.environments import (                      # noqa: E402
    activation_inputs_dir, install_key, install_state_dir, installs_root,
    runtime_facts_path, venv_bin_dir, venv_python,
)

# Roots that exist, so Path.resolve() is stable.
PROJ = "C:\\Users\\eman5\\hermes-agent"


def norm(p):
    return str(p).replace("\\", "/")


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        proj = i.get("project_root") or PROJ
        token = set_hermes_home_override(i.get("root"))
        try:
            if fn == "install_key":
                want = install_key(Path(proj))
            elif fn == "installs_root":
                want = norm(installs_root())
            elif fn == "install_state_dir":
                want = norm(install_state_dir(Path(proj)))
            elif fn == "runtime_facts_path":
                want = norm(runtime_facts_path(Path(proj)))
            elif fn == "activation_inputs_dir":
                want = norm(activation_inputs_dir(Path(proj)))
            elif fn == "venv_bin_dir":
                want = norm(venv_bin_dir(Path(i["venv"]),
                                         windows=bool(i.get("windows"))))
            elif fn == "venv_python":
                want = norm(venv_python(Path(i["venv"]),
                                        windows=bool(i.get("windows"))))
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        finally:
            reset_hermes_home_override(token)
        raw = c["out"]
        got = raw["str"] if isinstance(raw, dict) and set(raw) == {"str"} else raw
        got = norm(got)
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:200]} "
                  f"PY={want}")
    if total == 0:
        print("ORACLE pm_environments: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE pm_environments: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
