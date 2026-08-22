#!/usr/bin/env python3
"""sta_oracle_gitlock.py — Python twin for src/hermes_cli/gitlock.c.

Imports the LIVE hermes_cli/gitlock.py and mirrors the C ops, injecting the
two environmental inputs (wall clock + git-process probe) exactly like the C
seam does:
  {"kind":"clear","repo":"r1","min_age":-1,"now":E,"git_running":0}
  {"kind":"ancestor","repo":"rgit","rev":"HEAD"}
Emits one JSON line per op (same order).
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

PY_ROOT = str(Path.home() / ".hermes" / "hermes-agent")
if not os.path.isdir(PY_ROOT):
    PY_ROOT = "/home/wubu/hermes-agent-dev"
sys.path.insert(0, PY_ROOT)


def _load(name, relpath):
    if name in sys.modules:
        return sys.modules[name]
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(PY_ROOT, relpath))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


LOCK_MTIME = 1700000000  # fixed old mtime stamped by gen_fixture.sh


def main():
    gitlock = _load("hermes_cli.gitlock", "hermes_cli/gitlock.py")
    fixture = os.path.abspath(sys.argv[1])
    fixdir = os.environ.get("ORACLE_FIXDIR") or os.path.dirname(fixture)
    # The C phase mutates the fixture repos (removes locks); regenerate the
    # fixture so the Python phase starts from the same fresh state the C
    # harness saw.
    import subprocess as _sp
    _sp.run(["bash", os.path.join(fixdir, "gen_fixture.sh")], check=True)
    with open(fixture) as f:
        root = json.load(f)

    class FakeTime:
        @staticmethod
        def time():
            return injected_now

    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "clear":
            repo = Path(fixdir) / c.get("repo", "")
            global injected_now
            injected_now = float(c.get("now", 0))
            real_time = gitlock.time
            real_probe = gitlock._git_proc_running
            gitlock.time = FakeTime
            gitlock._git_proc_running = (
                lambda: bool(c.get("git_running", 0)))
            try:
                removed = gitlock.clear_stale_git_locks(
                    repo,
                    min_age_seconds=c.get("min_age"),
                )
            except Exception:
                removed = []
            finally:
                gitlock.time = real_time
                gitlock._git_proc_running = real_probe
            out = [str(x).replace(str(fixdir) + os.sep, "") for x in removed]
        elif kind == "ancestor":
            repo = Path(fixdir) / c.get("repo", "")
            out = gitlock.is_ancestor_of_head(repo, c.get("rev", ""))
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False))


if __name__ == "__main__":
    main()
