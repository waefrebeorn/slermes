#!/usr/bin/env python3
"""Differential oracle for the estimate / config / home-channel / diagnostics
projections of plugins/kanban/dashboard/plugin_api.py (gate
w27_pluginapi_1215d9_p3).

The live module is imported by file path.  Five seams are substituted per case:

  * ``agent.auxiliary_client.call_llm`` + ``agent.portal_tags`` — the aux model;
  * ``_load_config_or_empty`` — config.yaml;
  * ``gateway.config.load_gateway_config`` — the gateway platform registry;
  * ``hermes_cli.profiles`` — the profile registry;
  * ``_compute_task_diagnostics`` + ``_board_conn`` — the board DB.

What is compared is the payload each handler returns once its I/O is in hand.
"""
import contextlib
import importlib.util
import json
import logging
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)  # the module's log.exception paths are exercised, not asserted

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_pluginapi_1215d9_p3"

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
        "kanban_plugin_api_p3_under_test",
        QUARRY / "plugins" / "kanban" / "dashboard" / "plugin_api.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


live = load_live()
kb = live.kanban_db
kbn = live.kbn
hc = sys.modules["hermes_cli"]
NS = types.SimpleNamespace

WANTED = {"cap", "estimate", "config_shape", "home_channels", "home_for_platform",
          "home_channels_subscribed", "active_profile", "model_options",
          "diagnostics_list"}


def ok(v):
    return {"ok": True, "value": v}


def wrap(fn):
    try:
        return ok(fn())
    except live.HTTPException as e:
        return {"ok": False, "kind": "http", "status": e.status_code, "detail": e.detail}
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "kind": "exc", "exc": type(e).__name__, "detail": str(e)}


# --- aux LLM seam ----------------------------------------------------------

import hermes_cli.profiles  # noqa: E402  — load the registry before the stubs

try:
    import agent as AGENT  # the real package, so its other submodules still import
except Exception:  # noqa: BLE001
    AGENT = types.ModuleType("agent")
    AGENT.__path__ = []


class _Msg:
    def __init__(self, content):
        self.content = content


class _Choice:
    def __init__(self, content):
        self.message = _Msg(content)


class _Resp:
    def __init__(self, content, model):
        self.choices = [_Choice(content)]
        self.model = model


AUX = types.ModuleType("agent.auxiliary_client")
PORTAL = types.ModuleType("agent.portal_tags")
AGENT.auxiliary_client = AUX
AGENT.portal_tags = PORTAL
PORTAL.get_affinity_scope = lambda: None
PORTAL.set_affinity_scope = lambda key: "affinity-token"
PORTAL.reset_affinity_scope = lambda tok: None
sys.modules["agent"] = AGENT
sys.modules["agent.auxiliary_client"] = AUX
sys.modules["agent.portal_tags"] = PORTAL

LLM_ERRORS = {"RuntimeError": RuntimeError, "ValueError": ValueError,
              "TimeoutError": TimeoutError, "KeyError": KeyError}


@contextlib.contextmanager
def fake_board_conn(board=None):
    yield (None, NS(execute=lambda *a: NS(fetchall=lambda: [])))


# --- per-function expectations ---------------------------------------------

def cap(r):
    return ok(live._cap(r.get("s"), r.get("n", 0)))


def estimate(r):
    def call_llm(**kw):
        if r.get("llm_error"):
            raise LLM_ERRORS.get(r["llm_error"], RuntimeError)("upstream boom")
        return _Resp(r.get("raw") or "", r.get("model"))

    AUX.call_llm = call_llm
    if r.get("aux") is False:
        # None in sys.modules makes the lazy "from agent.auxiliary_client import
        # call_llm" raise ImportError -> "auxiliary client unavailable".
        sys.modules["agent.auxiliary_client"] = None
        try:
            return ok(live._run_estimate(r.get("title"), r.get("body"), task_id=None))
        finally:
            sys.modules["agent.auxiliary_client"] = AUX
    return ok(live._run_estimate(r.get("title"), r.get("body"), task_id=None))


def config_shape(r):
    orig = live._load_config_or_empty
    live._load_config_or_empty = lambda: r.get("config") or {}
    try:
        return ok(live.get_config())
    finally:
        live._load_config_or_empty = orig


class _Plat:
    """Hashable stand-in for the gateway Platform enum."""

    def __init__(self, value):
        self.value = value


def _platforms(raw):
    out = {}
    for p in raw or []:
        if not p:
            continue
        home = p.get("home_channel")
        pcfg = NS(home_channel=None) if home is None else NS(
            home_channel=NS(chat_id=home.get("chat_id"), thread_id=home.get("thread_id"),
                            name=home.get("name")))
        out[_Plat(p.get("value"))] = pcfg
    return out


def home_channels(r):
    gwc = types.ModuleType("gateway.config")
    gwc.load_gateway_config = lambda: NS(platforms=_platforms(r.get("platforms")))
    gw = types.ModuleType("gateway")
    gw.config = gwc
    saved = {k: sys.modules.get(k) for k in ("gateway", "gateway.config")}
    sys.modules["gateway"] = gw
    sys.modules["gateway.config"] = gwc
    try:
        return ok(live._configured_home_channels())
    finally:
        for k, v in saved.items():
            if v is None:
                sys.modules.pop(k, None)
            else:
                sys.modules[k] = v


def home_for_platform(r):
    orig = live._configured_home_channels
    live._configured_home_channels = lambda: r.get("homes") or []
    try:
        return wrap(lambda: live._home_for_platform(r.get("platform"), r.get("detail")))
    finally:
        live._configured_home_channels = orig


def home_channels_subscribed(r):
    orig_cfg, orig_conn, orig_subs = (live._configured_home_channels, live._board_conn,
                                      kbn.list_notify_subs)
    live._configured_home_channels = lambda: r.get("homes") or []
    live._board_conn = fake_board_conn
    kbn.list_notify_subs = lambda conn, task_id: r.get("subs") or []
    try:
        return ok(live.get_home_channels(r.get("task_id"), None))
    finally:
        live._configured_home_channels, live._board_conn = orig_cfg, orig_conn
        kbn.list_notify_subs = orig_subs


def active_profile(r):
    import hermes_cli.profiles as profiles_mod

    def fake():
        if r.get("raise"):
            raise RuntimeError("registry down")
        return r.get("profile")

    orig = profiles_mod.get_active_profile_name
    profiles_mod.get_active_profile_name = fake
    try:
        return ok(live._active_profile_name())
    finally:
        profiles_mod.get_active_profile_name = orig


def model_options(r):
    inv = types.ModuleType("hermes_cli.inventory")
    inv.load_picker_context = lambda: {}

    def build_models_payload(ctx, **kw):
        if r.get("raise"):
            raise RuntimeError("catalog down")
        return r.get("payload") or {}

    inv.build_models_payload = build_models_payload
    saved_attr = getattr(hc, "inventory", None)
    saved_mod = sys.modules.get("hermes_cli.inventory")
    hc.inventory = inv
    sys.modules["hermes_cli.inventory"] = inv
    try:
        return ok(live.model_options())
    finally:
        if saved_mod is None:
            sys.modules.pop("hermes_cli.inventory", None)
        else:
            sys.modules["hermes_cli.inventory"] = saved_mod
        if saved_attr is None:
            del hc.inventory
        else:
            hc.inventory = saved_attr


def diagnostics_list(r):
    rows = []
    for tid, row in (r.get("rows") or {}).items():
        merged = {"id": tid}
        merged.update(row)
        rows.append(merged)

    @contextlib.contextmanager
    def board_conn(board=None):
        yield (None, NS(execute=lambda *a: NS(fetchall=lambda: list(rows))))

    orig_diag, orig_conn = live._compute_task_diagnostics, live._board_conn
    live._compute_task_diagnostics = lambda conn, task_ids=None: r.get("diags_by_task") or {}
    live._board_conn = board_conn
    try:
        return ok(live.list_diagnostics(None, r.get("severity")))
    finally:
        live._compute_task_diagnostics, live._board_conn = orig_diag, orig_conn


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
