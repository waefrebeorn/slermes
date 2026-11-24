#!/usr/bin/env python3
"""Differential oracle for gate w18_kanbanresu_d42cf6 (hermes_cli/kanban_resume_guard.py)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "kanban_resume_guard.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)

CTX = {}
_gw = types.ModuleType("gateway")
_gw.__path__ = []
sys.modules["gateway"] = _gw
_sc = types.ModuleType("gateway.session_context")
_sc.get_session_env = lambda key, default="": (
    CTX.get("task_value") if CTX.get("task_value") is not None else default)
sys.modules["gateway.session_context"] = _sc
_ag = types.ModuleType("agent")
_ag.__path__ = []
sys.modules["agent"] = _ag
_i18n = types.ModuleType("agent.i18n")
_i18n.t = lambda key, **kw: "BLOCKED:" + str(kw.get("session_id"))
sys.modules["agent.i18n"] = _i18n

_SPEC = importlib.util.spec_from_file_location(
    "w18_kanbanresu_live", str(Path(QUARRY) / "hermes_cli" / "kanban_resume_guard.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


class _DbProbe:
    def __init__(self, result, raises=False):
        self.result = result
        self.raises = raises

    def is_kanban_owned_session(self, session_id):
        if self.raises:
            raise RuntimeError("simulated probe failure")
        return self.result


class _DbPlain:
    pass


class _Sessions:
    def __init__(self, result, raises=False):
        self.result = result
        self.raises = raises

    def is_kanban_owned(self, session_id):
        if self.raises:
            raise RuntimeError("simulated persistence failure")
        return self.result


class _Persistence:
    def __init__(self, result, raises=False):
        self.sessions = _Sessions(result, raises)


def expected(rec):
    f = rec.get("func")
    CTX["task_value"] = rec.get("task_value")
    if f == "dispatcher_owned_run":
        return mod._dispatcher_owned_run()
    if f == "kanban_resume_refusal":
        mode = rec.get("mode")
        db = _DbPlain()
        persistence = None
        if mode == "probe_true":
            db = _DbProbe(True)
        elif mode == "probe_false":
            db = _DbProbe(False)
        elif mode == "probe_other":
            db = _DbProbe(1)
        elif mode == "probe_none":
            db = _DbProbe(None)
        elif mode == "probe_raises":
            db = _DbProbe(None, raises=True)
        elif mode == "persistence_true":
            persistence = _Persistence(True)
        elif mode == "persistence_false":
            persistence = _Persistence(False)
        elif mode == "persistence_raises":
            persistence = _Persistence(None, raises=True)
        return mod.kanban_resume_refusal(db, rec.get("session_id"), persistence=persistence)
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        want = expected(rec)
        if got != want:
            mism += 1
            if mism <= 10:
                print("MISMATCH %s case=%s got=%r want=%r" %
                      (rec.get("func"), rec.get("case"), got, want), file=sys.stderr)
    print("ORACLE w18_kanbanresu_d42cf6: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
