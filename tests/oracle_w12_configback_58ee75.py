#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_backups.py (gate w12_configback_58ee75)."""
import importlib.util
import json
import sys
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_configback_58ee75"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/hermes_cli/config_backups.py"
_spec = importlib.util.spec_from_file_location("config_backups_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["config_backups_live"] = live
_spec.loader.exec_module(live)

_REAL_BACKUPS_DIR = live.backups_dir


class _File:
    def __init__(self, name, root):
        self.name = name
        self._root = root

    def is_file(self):
        return True

    def as_posix(self):
        return self._root.rstrip("/") + "/" + self.name

    def __str__(self):
        return self.as_posix()


class _Root:
    def __init__(self, root, names):
        self._root = root
        self._names = names

    def is_dir(self):
        return True

    def iterdir(self):
        return iter([_File(n, self._root) for n in self._names])


def live_out(rec):
    func = rec.get("func")
    config = rec.get("config", "")
    if func == "dir":
        return _REAL_BACKUPS_DIR(Path(config)).as_posix()
    if func == "list":
        names = [n for n in (rec.get("files") or []) if isinstance(n, str)]
        root = _REAL_BACKUPS_DIR(Path(config)).as_posix()
        live.backups_dir = lambda cp: _Root(root, names)
        try:
            reason = rec.get("reason") or None
            res = live.list_config_backups(Path(config), reason)
            return [p.as_posix() for p in res]
        finally:
            live.backups_dir = _REAL_BACKUPS_DIR
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
        if rec.get("func") not in ("dir", "list"):
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
