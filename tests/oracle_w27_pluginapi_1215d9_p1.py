#!/usr/bin/env python3
"""Differential oracle for the request/response plumbing helpers of
plugins/kanban/dashboard/plugin_api.py (gate w27_pluginapi_1215d9_p1).

The live module is imported by file path (its package __init__ drags the whole
gateway stack) and its pure helpers are called directly.  Three seams are
substituted per case, never mocked wholesale:

  * ``kanban_db.board_exists`` / ``read_board_metadata`` — the board registry;
  * ``kanban_db.get_task`` — the linked-task lookup;
  * ``kanban_db.time`` — the clock behind ``task_age``.

What is compared is exactly what the port computes: the payload shape, the
derived age metrics, the diagnostic badge rollup and the 404/409/500 text.
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
GATE = "w27_pluginapi_1215d9_p1"

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
        "kanban_plugin_api_p1_under_test",
        QUARRY / "plugins" / "kanban" / "dashboard" / "plugin_api.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


live = load_live()
kb = live.kanban_db
NS = types.SimpleNamespace

WANTED = {
    "normalize_slug", "resolve_board", "existing_board_slug", "placeholders",
    "attachment_dict", "task_dict",
    "warnings_summary", "attach_diagnostics", "require_obj", "require_ok", "conflict",
    "map_errors", "errors_to_500", "link_tasks", "module_constants",
}

TASK_FIELDS = [f for f in kb.Task.__dataclass_fields__]
TASK_REQUIRED = [
    name for name, f in kb.Task.__dataclass_fields__.items()
    if f.default is __import__("dataclasses").MISSING
    and f.default_factory is __import__("dataclasses").MISSING]


def ok(v):
    return {"ok": True, "value": v}


def http(e):
    return {"ok": False, "kind": "http", "status": e.status_code, "detail": e.detail}


def wrap(fn):
    try:
        return ok(fn())
    except live.HTTPException as e:
        return http(e)
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "kind": "exc", "exc": type(e).__name__, "detail": str(e)}


# --- per-function expectations ---------------------------------------------

def normalize_slug(r):
    return wrap(lambda: live._normalize_slug_or_400(r.get("slug")))


def resolve_board(r):
    orig_e, orig_m = kb.board_exists, kb.read_board_metadata
    kb.board_exists = lambda slug: bool(r.get("exists"))
    kb.read_board_metadata = lambda slug: {"archived": bool(r.get("archived"))}
    try:
        return wrap(lambda: live._resolve_board(r.get("board")))
    finally:
        kb.board_exists, kb.read_board_metadata = orig_e, orig_m


def placeholders(r):
    return ok(live._placeholders(r.get("ids") or []))


def existing_board_slug(r):
    orig = kb.board_exists
    kb.board_exists = lambda slug: bool(r.get("exists"))
    try:
        return wrap(lambda: live._existing_board_slug(r.get("slug")))
    finally:
        kb.board_exists = orig


def attachment_dict(r):
    keys = ("id", "task_id", "filename", "content_type", "size", "uploaded_by",
            "stored_path", "created_at")
    att = {k: None for k in keys}
    att.update(r.get("attachment") or {})
    return ok(live._attachment_dict(NS(**att)))


def task_dict(r):
    raw = r.get("task") or {}
    kwargs = {k: raw[k] for k in TASK_FIELDS if k in raw}
    for k in TASK_REQUIRED:
        kwargs.setdefault(k, None)
    task = kb.Task(**kwargs)
    orig = kb.time
    kb.time = NS(time=lambda: r.get("now", 0))
    try:
        return ok(live._task_dict(
            task,
            latest_summary=r.get("latest_summary"),
            current_run_started_at=r.get("current_run_started_at")))
    finally:
        kb.time = orig


def warnings_summary(r):
    return ok(live._warnings_summary_from_diagnostics(r.get("diagnostics") or []))


def attach_diagnostics(r):
    task_d = dict(r.get("task") or {})
    live._attach_diagnostics(task_d, r.get("diagnostics"))
    return ok(task_d)


def require_obj(r):
    obj = r.get("obj") if r.get("found") else None
    return wrap(lambda: live._require(lambda conn, ident: obj, None, r.get("ident"),
                                      r.get("label")))


def require_ok(r):
    return wrap(lambda: live._require_ok(r.get("ok")))


def conflict(r):
    return wrap(lambda: (_ for _ in ()).throw(live._conflict(r.get("detail"))))


EXC_TYPES = {
    "ValueError": ValueError,
    "RuntimeError": RuntimeError,
    "KeyError": KeyError,
    "_StatusRejected": live._StatusRejected,
}


def map_errors(r):
    types_ = tuple(EXC_TYPES.get(t, Exception) for t in (r.get("types") or []))

    def body():
        with live._map_errors(r.get("status"), *types_):
            if r.get("exc") == "HTTPException":
                raise live.HTTPException(status_code=r.get("http_status"),
                                         detail=r.get("http_detail"))
            raise EXC_TYPES.get(r.get("exc"), Exception)(r.get("message"))

    return wrap(body)


def errors_to_500(r):
    def body():
        with live._errors_to_500(r.get("prefix")):
            if r.get("exc") == "HTTPException":
                raise live.HTTPException(status_code=r.get("http_status"),
                                         detail=r.get("http_detail"))
            raise EXC_TYPES.get(r.get("exc"), Exception)(r.get("message"))

    return wrap(body)


def link_tasks(r):
    tasks = r.get("tasks") or {}
    orig = kb.get_task

    def fake_get_task(conn, task_id):
        row = tasks.get(task_id)
        if row is None:
            return None
        return NS(id=task_id, title=row.get("title"), status=row.get("status"))

    kb.get_task = fake_get_task
    try:
        return ok(live._link_tasks(None, r.get("links") or {}))
    finally:
        kb.get_task = orig


def module_constants(r):
    return ok({
        "board_columns": list(live.BOARD_COLUMNS),
        "card_summary_preview_chars": live._CARD_SUMMARY_PREVIEW_CHARS,
        "event_poll_seconds": live._EVENT_POLL_SECONDS,
        "running_direct_msg": live._RUNNING_DIRECT_MSG,
    })


DISPATCH = {
    "normalize_slug": normalize_slug,
    "resolve_board": resolve_board,
    "existing_board_slug": existing_board_slug,
    "placeholders": placeholders,
    "attachment_dict": attachment_dict,
    "task_dict": task_dict,
    "warnings_summary": warnings_summary,
    "attach_diagnostics": attach_diagnostics,
    "require_obj": require_obj,
    "require_ok": require_ok,
    "conflict": conflict,
    "map_errors": map_errors,
    "errors_to_500": errors_to_500,
    "link_tasks": link_tasks,
    "module_constants": module_constants,
}


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
