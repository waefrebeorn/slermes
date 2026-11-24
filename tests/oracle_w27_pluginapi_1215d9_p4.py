#!/usr/bin/env python3
"""Differential oracle for the board / profile / orchestration / websocket
projections of plugins/kanban/dashboard/plugin_api.py (gate
w27_pluginapi_1215d9_p4).

The live module is imported by file path.  Five seams are substituted per case:

  * ``hermes_cli.projects_db`` — the projects registry;
  * ``kanban_db_workspace._git_toplevel`` — the git probe behind the workspace hint;
  * ``hermes_cli.profiles`` — the profile roster / existence check;
  * ``kanban_db.list_boards`` + friends — the board list, current pointer and counts;
  * ``_load_config_or_empty`` — config.yaml.

What is compared is the annotated payload each handler returns.
"""
import importlib.util
import json
import logging
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_pluginapi_1215d9_p4"

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
        "kanban_plugin_api_p4_under_test",
        QUARRY / "plugins" / "kanban" / "dashboard" / "plugin_api.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


live = load_live()
kb = live.kanban_db
kbw = live.kbw
hc = sys.modules["hermes_cli"]
import hermes_cli.profiles as profiles_mod  # noqa: E402

NS = types.SimpleNamespace

WANTED = {"board_display_kwargs", "resolve_project", "default_workspace_kind",
          "annotate_board_meta", "validated_profile_name", "orchestration_settings",
          "since_param", "ws_board", "ws_board_live", "list_boards_annotate",
          "profile_roster"}


def ok(v):
    return {"ok": True, "value": norm(v)}


def norm(v):
    """JSON-normalise: the wire form has no tuples, so neither does the compare."""
    if isinstance(v, tuple):
        return [norm(x) for x in v]
    if isinstance(v, list):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    return v


def wrap(fn):
    try:
        return ok(fn())
    except live.HTTPException as e:
        return {"ok": False, "kind": "http", "status": e.status_code, "detail": e.detail}
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "kind": "exc", "exc": type(e).__name__, "detail": str(e)}


# --- per-function expectations ---------------------------------------------

def board_display_kwargs(r):
    payload = {"slug": "b"}
    payload.update(r.get("payload") or {})
    return ok(live._board_display_kwargs(live.CreateBoardBody(**payload)))


def resolve_project(r):
    projects = r.get("projects") or {}
    pdb = types.ModuleType("hermes_cli.projects_db")

    def connect_closing():
        class Ctx:
            def __enter__(self):
                return None

            def __exit__(self, *a):
                return False

        return Ctx()

    def get_project(conn, ref):
        if r.get("connect_error"):
            raise RuntimeError(r.get("connect_message") or "connect failed")
        row = projects.get(ref)
        return None if row is None else NS(**row)

    pdb.connect_closing = connect_closing
    pdb.get_project = get_project
    saved_attr = getattr(hc, "projects_db", None)
    saved_mod = sys.modules.get("hermes_cli.projects_db")
    hc.projects_db = pdb
    sys.modules["hermes_cli.projects_db"] = pdb
    try:
        return wrap(lambda: live._resolve_project(r.get("ref")))
    finally:
        if saved_mod is None:
            sys.modules.pop("hermes_cli.projects_db", None)
        else:
            sys.modules["hermes_cli.projects_db"] = saved_mod
        if saved_attr is None:
            del hc.projects_db
        else:
            hc.projects_db = saved_attr


def default_workspace_kind(r):
    git = r.get("git")
    orig = kbw._git_toplevel

    def fake(path):
        if git is None:
            raise OSError("no git here")
        return "/repo" if git else None

    kbw._git_toplevel = fake
    try:
        return ok(live._default_workspace_kind(r.get("board") or {}))
    finally:
        kbw._git_toplevel = orig


def annotate_board_meta(r):
    project = r.get("project")
    orig_kind, orig_proj = live._default_workspace_kind, live._resolve_project
    live._default_workspace_kind = lambda board: r.get("workspace_kind")
    live._resolve_project = lambda ref: tuple(project) if project else (None, None, None)
    try:
        return ok(live._annotate_board_meta(dict(r.get("meta") or {})))
    finally:
        live._default_workspace_kind, live._resolve_project = orig_kind, orig_proj


def validated_profile_name(r):
    class Profiles:
        def profile_exists(self, name):
            if r.get("exists_raises"):
                raise RuntimeError("registry down")
            return bool(r.get("exists", True))

    mod = Profiles() if r.get("mod", True) is not False else None
    return wrap(lambda: live._validated_profile_name(r.get("raw"), mod))


def orchestration_settings(r):
    orig_cfg = live._load_config_or_empty
    orig_active = profiles_mod.get_active_profile_name
    orig_exists = profiles_mod.profile_exists
    known = set(r.get("exists") or [])

    def fake_active():
        if r.get("active_raises"):
            raise RuntimeError("registry down")
        return r.get("active")

    live._load_config_or_empty = lambda: r.get("config") or {}
    profiles_mod.get_active_profile_name = fake_active
    profiles_mod.profile_exists = lambda name: name in known
    try:
        return ok(live.get_orchestration_settings())
    finally:
        live._load_config_or_empty = orig_cfg
        profiles_mod.get_active_profile_name = orig_active
        profiles_mod.profile_exists = orig_exists


def since_param(r):
    return ok(live._since_param(NS(query_params=dict(r.get("query") or {}))))


def ws_board(r):
    return ok(live._ws_board(r.get("raw")))


def ws_board_live(r):
    boards = set(r.get("boards") or [])
    orig = kb.board_exists
    kb.board_exists = lambda slug: slug in boards
    try:
        return ok(live._ws_board_live(r.get("slug")))
    finally:
        kb.board_exists = orig


def list_boards_annotate(r):
    boards = [dict(b) for b in (r.get("boards") or [])]
    projects = r.get("projects") or {}
    counts = r.get("counts") or {}
    kinds = r.get("kinds") or {}
    orig = (kb.list_boards, kb.get_current_board, live._projects_by_id,
            live._board_counts, live._default_workspace_kind)
    kb.list_boards = lambda include_archived=False: [dict(b) for b in boards]
    kb.get_current_board = lambda: r.get("current")
    live._projects_by_id = lambda: {
        pid: NS(name=(row.get("name") if isinstance(row, dict) else row))
        for pid, row in projects.items()}
    live._board_counts = lambda slug: dict(counts.get(slug) or {})
    live._default_workspace_kind = lambda board: kinds.get(board.get("slug")) or "scratch"
    try:
        return ok(live.list_boards(include_archived=False))
    finally:
        (kb.list_boards, kb.get_current_board, live._projects_by_id,
         live._board_counts, live._default_workspace_kind) = orig


def profile_roster(r):
    rows = [NS(**p) for p in (r.get("profiles") or [])]
    orig = profiles_mod.list_profiles
    profiles_mod.list_profiles = lambda: rows
    try:
        return ok(live.list_profile_roster())
    finally:
        profiles_mod.list_profiles = orig


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
