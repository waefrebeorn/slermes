#!/usr/bin/env python3
"""Differential oracle for hermes_cli/session_worktree.py (gate w15_sessionwor_066f13)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_sessionwor_066f13"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_hc = types.ModuleType("hermes_cli")
_hc.__path__ = []
sys.modules["hermes_cli"] = _hc
_p = types.ModuleType("persistence")
_p.__path__ = []
sys.modules["persistence"] = _p
_pc = types.ModuleType("persistence.core")
_pc.__path__ = []
sys.modules["persistence.core"] = _pc
_mod("persistence.core.errors", PersistenceError=type("PersistenceError", (Exception,), {}))

_state = {"policy": None, "record": None}


def _make_db():
    def receipt(sid):
        return types.SimpleNamespace(policy=_state["policy"])
    rec = _state["record"]
    return types.SimpleNamespace(persistence=types.SimpleNamespace(
        bindings=types.SimpleNamespace(receipt=receipt),
        sessions=types.SimpleNamespace(get=lambda sid: (types.SimpleNamespace(cwd=rec) if rec is not None else None)),
    ))


class _DB:
    def __enter__(self):
        return _make_db()

    def __exit__(self, *args):
        return False


_mod("hermes_cli.main", _session_db=lambda: _DB())


def _recreate(cwd, sid):
    return _state["recreated"]


_mod("hermes_cli.tui_worktree_resume", recreate_tui_worktree=_recreate)

SRC = QUARRY + "/hermes_cli/session_worktree.py"
_spec = importlib.util.spec_from_file_location("session_worktree_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["session_worktree_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    f = rec.get("func")
    if f == "frozen_cwd":
        _state["policy"] = rec.get("policy")
        return live._frozen_cwd(_make_db(), "sid")
    if f == "restore":
        _state["policy"] = {"cwd": rec.get("frozen", "")}
        _state["record"] = rec.get("record_cwd")
        _state["recreated"] = rec.get("recreated")
        live._frozen_cwd = lambda db, sid: (rec.get("frozen", "") or "").strip()
        live.os = types.SimpleNamespace(path=types.SimpleNamespace(isdir=lambda p: bool(rec.get("is_dir", False))))
        return live.restore("sid")
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
        if rec.get("func") not in ("frozen_cwd", "restore"):
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
