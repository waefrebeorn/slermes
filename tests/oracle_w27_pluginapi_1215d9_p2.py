#!/usr/bin/env python3
"""Differential oracle for the status-verb half of
plugins/kanban/dashboard/plugin_api.py (gate w27_pluginapi_1215d9_p2).

The live module is imported by file path.  Every mutator that would touch the
state DB is substituted with a recorder on the ``kanban_db`` module, so what is
compared is the decision the dashboard makes — which verb a status names, which
verb a drag lands on, the override value it would persist, and the exact 400 /
409 / _StatusRejected text the UI renders.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_pluginapi_1215d9_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


def install_import_stubs():
    """Pre-seed the third-party modules the truth module only touches at its
    I/O boundary.

    ``plugin_api.py`` imports ``fastapi`` and ``pydantic`` purely for its route
    decorators, exception type and request models: none of the pure helpers
    below needs either wheel.  On hosts whose application-control policy
    refuses pydantic's compiled extension the real import raises ImportError
    long before any behaviour is exercised, so we supply the exact surface the
    module references — BaseModel (kwargs -> attributes, missing -> None),
    Field (defaults), a ValueError-shaped ValidationError, HTTPException with
    ``status_code``/``detail``, a no-op APIRouter and the File/Form/Query
    parameter helpers.  The real wheels are used whenever they import cleanly.
    """
    import os

    force = os.environ.get("KP_FORCE_IMPORT_STUBS") == "1"
    have_pyd = have_fa = False
    if not force:
        try:
            import pydantic  # noqa: F401
            have_pyd = True
        except Exception:  # noqa: BLE001
            pass
        try:
            import fastapi  # noqa: F401
            have_fa = True
        except Exception:  # noqa: BLE001
            pass
        if have_pyd and have_fa:
            return "real"

    if not have_pyd:
        pyd = types.ModuleType("pydantic")

        class BaseModel:
            def __init__(self, **kwargs):
                for k, v in kwargs.items():
                    setattr(self, k, v)

            def __getattr__(self, item):
                return None  # Optional[...] = None fields

            def model_dump(self, exclude_none=False, **kw):
                out = dict(self.__dict__)
                if exclude_none:
                    out = {k: v for k, v in out.items() if v is not None}
                return out

        def Field(default=None, *, default_factory=None, **kw):
            return default_factory() if default_factory is not None else default

        class ValidationError(ValueError):
            pass

        def field_validator(*a, **kw):
            def deco(fn):
                return fn
            return deco

        pyd.BaseModel = BaseModel
        pyd.Field = Field
        pyd.ValidationError = ValidationError
        pyd.field_validator = field_validator
        pyd.ConfigDict = dict
        sys.modules["pydantic"] = pyd

    if not have_fa:
        fa = types.ModuleType("fastapi")
        fa.responses = types.ModuleType("fastapi.responses")

        class HTTPException(Exception):
            def __init__(self, status_code=500, detail=None):
                self.status_code = status_code
                self.detail = detail
                super().__init__("%s: %s" % (status_code, detail))

        class _Router:
            def get(self, *a, **kw):
                return lambda fn: fn

            post = patch = put = delete = websocket = get

        def Query(default=None, **kw):
            return default

        def File(default=None, **kw):
            return default

        def Form(default=None, **kw):
            return default

        class UploadFile:
            pass

        class WebSocket:
            pass

        class WebSocketDisconnect(Exception):
            pass

        class FileResponse:
            def __init__(self, *a, **kw):
                pass

        fa.APIRouter = _Router
        fa.HTTPException = HTTPException
        fa.Query, fa.File, fa.Form = Query, File, Form
        fa.UploadFile, fa.WebSocket = UploadFile, WebSocket
        fa.WebSocketDisconnect = WebSocketDisconnect
        fa.status = types.SimpleNamespace(WS_1008_POLICY_VIOLATION=1008)
        fa.responses.FileResponse = FileResponse
        sys.modules["fastapi"] = fa
        sys.modules["fastapi.responses"] = fa.responses

    return "stub"


IMPORT_MODE = install_import_stubs()


def load_live():
    spec = importlib.util.spec_from_file_location(
        "kanban_plugin_api_p2_under_test",
        QUARRY / "plugins" / "kanban" / "dashboard" / "plugin_api.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


live = load_live()
kb = live.kanban_db
NS = types.SimpleNamespace

WANTED = {
    "status_refusal", "status_table", "patch_status_route", "open_parent_refusal",
    "model_override_value", "reasoning_effort_value", "override_wanted", "drag_route",
    "bulk_ids", "comment_body_gate", "review_assignee_deferred",
}


def ok(v):
    return {"ok": True, "value": v}


def wrap(fn):
    try:
        return ok(fn())
    except live.HTTPException as e:
        return {"ok": False, "kind": "http", "status": e.status_code, "detail": e.detail}
    except live._StatusRejected as e:
        return {"ok": False, "kind": "exc", "exc": "_StatusRejected", "detail": str(e)}
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "kind": "exc", "exc": type(e).__name__, "detail": str(e)}


# --- per-function expectations ---------------------------------------------

def status_refusal(r):
    s = r.get("status")
    try:
        live._apply_status(object(), "t", s, NS(), r.get("unknown_detail"))
    except live._StatusRejected as e:
        return {"ok": False, "kind": "exc", "exc": "_StatusRejected", "detail": str(e)}
    except Exception:  # noqa: BLE001  -- a real handler ran on a fake conn
        pass
    return ok(True)


def status_table(r):
    return ok(list(live._STATUS_HANDLERS))


def patch_status_route(r):
    seen = {}
    orig = kb.archive_task
    kb.archive_task = lambda conn, tid: seen.setdefault("archive", True)
    try:
        live._patch_status(None, "t", NS(status=r.get("status"), assignee=None,
                                         block_reason=None, summary=None, metadata=None,
                                         result=None), False)
    except Exception:  # noqa: BLE001  -- only reachable for the verb branch
        pass
    finally:
        kb.archive_task = orig
    return ok("archive" if seen else "verb")


def open_parent_refusal(r):
    blockers = [(p[0], p[1]) for p in (r.get("blockers") or [])
                if isinstance(p, list) and len(p) == 2]
    orig = kb.unsatisfied_parents
    kb.unsatisfied_parents = lambda conn, tid: blockers
    try:
        return ok(live._open_parent_refusal(None, r.get("task_id"), r.get("status")))
    finally:
        kb.unsatisfied_parents = orig


def model_override_value(r):
    cap = {}
    orig = kb.set_model_override

    def fake(conn, tid, model, provider=None):
        cap["model"] = model
        cap["provider"] = provider
        return True

    kb.set_model_override = fake
    try:
        live._apply_model_override(None, "t", NS(
            clear_model_override=bool(r.get("clear")),
            model_override=r.get("model_override"),
            provider_override=r.get("provider_override")))
    finally:
        kb.set_model_override = orig
    return ok({"model": cap.get("model"), "provider": cap.get("provider")})


def reasoning_effort_value(r):
    cap = {}
    orig = kb.set_reasoning_effort

    def fake(conn, tid, effort):
        cap["effort"] = effort
        return True

    kb.set_reasoning_effort = fake
    try:
        live._apply_reasoning_effort(None, "t", NS(
            clear_reasoning_effort=bool(r.get("clear")),
            reasoning_effort=r.get("reasoning_effort")))
    finally:
        kb.set_reasoning_effort = orig
    return ok(cap.get("effort"))


def override_wanted(r):
    op = int(r.get("op", 0))
    payload = NS(clear_model_override=False, model_override=None,
                 clear_reasoning_effort=False, reasoning_effort=None)
    field = "model_override" if op == 0 else "reasoning_effort"
    setattr(payload, "clear_" + field, bool(r.get("clear")))
    setattr(payload, field, r.get("value"))
    return ok(bool(live._OVERRIDE_OPS[op][0](payload)))


def drag_route(r):
    calls = []
    target = r.get("target")
    current = r.get("current_status")
    orig = (kb.get_task, kb.unblock_task, kb.reopen_review_task, live._set_status_direct)
    kb.get_task = lambda conn, tid: None if current is None else NS(status=current)
    kb.unblock_task = lambda conn, tid: calls.append("unblock") or True
    kb.reopen_review_task = lambda conn, tid: calls.append("reopen_review") or True
    live._set_status_direct = lambda conn, tid, s: calls.append("direct") or True
    try:
        live._drag_to(None, "t", target)
    finally:
        kb.get_task, kb.unblock_task, kb.reopen_review_task, live._set_status_direct = orig
    return ok(calls[0] if calls else "direct")


def bulk_ids(r):
    raw = r.get("ids")
    ids = [i for i in (raw if isinstance(raw, list) else []) if i]
    if not ids:
        try:
            live.bulk_update(live.BulkTaskBody(ids=list(raw or [])), None)
        except live.HTTPException as e:
            return {"ok": False, "kind": "http", "status": e.status_code, "detail": e.detail}
    return ok(ids)


def comment_body_gate(r):
    body = r.get("body")
    if not (body or "").strip():
        try:
            live.add_comment("t", live.CommentBody(body=body or ""), None)
        except live.HTTPException as e:
            return {"ok": False, "kind": "http", "status": e.status_code, "detail": e.detail}
    return ok(True)


def review_assignee_deferred(r):
    payload = live.UpdateTaskBody(status=r.get("status"), assignee=r.get("assignee"))
    return ok(payload.status == "review" and payload.assignee is not None)


DISPATCH = {name: globals()[name] for name in WANTED}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = DISPATCH[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r["func"], r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r" % (r["func"], r.get("case"), py, got),
                  file=sys.stderr)
    if not cases:
        # A missing/empty fixture must never look like a green gate.
        print("ORACLE %s: no cases on stdin" % GATE, file=sys.stderr)
        sys.exit(1)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
