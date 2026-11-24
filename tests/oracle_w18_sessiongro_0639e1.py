#!/usr/bin/env python3
"""Differential oracle for gateway/session_group_controls.py (w18_sessiongro_0639e1).

The live module is imported against stubbed persistence packages (its only
import-time dependencies), then each harness line is replayed through the live
helper with the same inputs. Compared values: the capability/field tables, the
wire-params validator, _refusal, _ui_meta_revisions, _stat_key, _has_avatar and
_execution_control's validation/receipt shaping against a scripted service.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class AuthorityRefusal(Exception):
    def __init__(self, reason, **kw):
        super().__init__(reason)
        self.reason = reason
        self.kw = kw


class StoreUnavailable(Exception):
    pass


for pkg in ("gateway", "persistence", "persistence.core"):
    if pkg not in sys.modules:
        _mk(pkg)
_mk("gateway.session_persistence", authority_persistence=lambda a: None, owner_rule=lambda: None)
_mk("persistence.core.errors", AuthorityRefusal=AuthorityRefusal, StoreUnavailable=StoreUnavailable)

NAME = "w18_sessiongro_0639e1_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "gateway", "session_group_controls.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class Obj(dict):
    """Dict with attribute access: `task['identity']` and `getattr(identity, ...)`."""
    __getattr__ = dict.__getitem__


def as_obj(v):
    if isinstance(v, dict):
        return Obj({k: as_obj(x) for k, x in v.items()})
    if isinstance(v, list):
        return [as_obj(x) for x in v]
    return v


def plain(v):
    if isinstance(v, dict):
        return {k: plain(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [plain(x) for x in v]
    return v


ERRORS = {"RuntimeError": RuntimeError, "ValueError": ValueError,
          "TypeError": TypeError, "KeyError": KeyError}


class Service:
    """The scripted hosted-room service the fixture describes."""

    def __init__(self, spec):
        self.spec = spec if isinstance(spec, dict) else {}

    def _maybe_raise(self):
        err = self.spec.get("error")
        if isinstance(err, dict):
            cls = ERRORS.get(err.get("type", "RuntimeError"), RuntimeError)
            raise cls(err.get("message", ""))

    def retry_room_task(self, **kw):
        self._maybe_raise()
        return as_obj(self.spec.get("task"))

    def discard_room_task(self, **kw):
        self._maybe_raise()
        return as_obj(self.spec.get("task"))

    def approve_room_task(self, **kw):
        self._maybe_raise()
        return as_obj(self.spec.get("result"))

    def continue_room(self, room_id, discussion_event_id=None):
        self._maybe_raise()
        return as_obj(self.spec.get("event"))


def outcome(fn):
    try:
        return {"status": "ok", "value": plain(fn())}
    except AuthorityRefusal as exc:
        return {"status": "refused", "reason": exc.reason,
                "admitted": False if getattr(exc, "admitted", None) is False else None}
    except Exception as exc:  # noqa: BLE001 - the harness reports raised exceptions verbatim
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}


def expected(r):
    f = r.get("func")
    if f == "capability_for_method":
        return live.GROUP_METHODS.get(r.get("method"), "session:read")
    if f == "fields_for_method":
        method = r.get("method")
        return sorted(live._FIELDS[method]) if method in live._FIELDS else None
    if f == "params_allowed":
        method, params = r.get("method"), r.get("params")
        return bool(method in live._FIELDS and isinstance(params, dict)
                    and not (set(params) - (live._FIELDS[method] | {"profile"})))
    if f == "refusal":
        refusal = live._refusal(r.get("reason"), admitted=r.get("admitted"))
        out = {"reason": refusal.reason}
        if getattr(refusal, "admitted", None) is False:
            out["admitted"] = False
        return out
    if f == "ui_meta_revisions":
        return live._ui_meta_revisions(r.get("meta"))
    if f == "stat_key":
        stat = types.SimpleNamespace(st_mtime_ns=int(r.get("mtime_ns")),
                                     st_ctime_ns=int(r.get("ctime_ns")),
                                     st_size=int(r.get("size")),
                                     st_ino=int(r.get("ino")))
        return [str(x) for x in live._stat_key(stat)]
    if f == "has_avatar":
        return bool(live._has_avatar(Path(r.get("home"))))
    if f == "execution_control":
        method = r.get("method")
        params = r.get("params") if isinstance(r.get("params"), dict) else {}
        service = Service(r.get("service"))
        return outcome(lambda: live._execution_control(service, method, dict(params)))
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w18_sessiongro_0639e1: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
