#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/oauth.py (gate w29_oauth_dc3549_p2).

The live router module is imported BY FILE PATH and its device-code values are
compared against the C port.  Every HTTP/files/thread seam is scripted:

  * `_codex_post` is replaced by a recorder that returns a canned response, so
    `_codex_request_user_code` (interval clamp + payload validation) and
    `_codex_exchange_tokens` (the token-exchange form) run with no socket;
  * the caller-passed `resp` object of `_http_response_error_detail` /
    `_codex_device_code_start_error` is a stub built from the fixture
    (status / parsed payload / text);
  * `_oauth_sessions` is a plain dict and `_start_poller` a recorder, so
    `_gc_oauth_sessions` and `_device_session_started` are observed without a
    thread or `secrets` (the session id is a fixture value);
  * `_resolve_profile_dir` is a recorder for `_validate_oauth_profile`.

Fixture-only conventions: the C side never invents values it cannot see, so the
session id is passed in and substituted into the live dict before comparison.
"""
import copy
import importlib.util
import json
import os
import sys
from pathlib import Path
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_oauth_dc3549_p2"

# hermes_constants resolves the home dir at import time; a bare `bash -lc` drops
# USERPROFILE/HOME and Path.home() then raises.  The repo's parent IS the home.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(QUARRY.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# --- host workaround: this box blocks pydantic's native extension ------------
# ImportError: DLL load failed while importing _pydantic_core: Access is denied.
# pydantic/fastapi are seams here (no model or HTTP logic under test), so stand
# them in wholesale instead of importing the host's copies.
import types as _types


def _stub_module(name, **attrs):
    mod = _types.ModuleType(name)
    for _k, _v in attrs.items():
        setattr(mod, _k, _v)
    sys.modules[name] = mod
    return mod


class _BaseModel:
    def __init__(self, **kw):
        self.__dict__.update(kw)

    @classmethod
    def model_validate(cls, obj):
        return cls(**(obj or {}))

    def model_dump(self, *a, **k):
        return dict(self.__dict__)


def _Field(default=None, *a, **k):
    return default


def _decorator(*a, **k):
    def wrap(fn):
        return fn
    return wrap


def _create_model(name, **fields):
    return type(name, (_BaseModel,), {})


class _ValidationError(ValueError):
    pass


_stub_module("pydantic", BaseModel=_BaseModel, Field=_Field,
             field_validator=_decorator, validator=_decorator,
             model_validator=_decorator, root_validator=_decorator,
             ConfigDict=dict, ValidationError=_ValidationError,
             SecretStr=str, create_model=_create_model)
_stub_module("pydantic.fields", Field=_Field)
_stub_module("pydantic.main", BaseModel=_BaseModel)
_stub_module("pydantic_core", PydanticUndefined=None)
_stub_module("pydantic_settings", BaseSettings=_BaseModel, SettingsConfigDict=dict)


class _HTTPException(Exception):
    def __init__(self, status_code=500, detail=None, headers=None):
        super().__init__(detail)
        self.status_code = status_code
        self.detail = detail
        self.headers = headers


class _APIRouter:
    def __init__(self, *a, **k):
        pass

    def _route(self, *a, **k):
        def deco(fn):
            return fn

        if a and callable(a[0]):
            return deco(a[0])
        return deco

    get = post = put = patch = delete = head = options = _route
    add_api_route = _route


class _Response:
    def __init__(self, content=None, status_code=200, **kw):
        self.content = content
        self.status_code = status_code


_stub_module("fastapi", APIRouter=_APIRouter, HTTPException=_HTTPException,
             Request=object, Response=_Response, JSONResponse=_Response,
             PlainTextResponse=_Response, Depends=lambda *a, **k: None,
             Query=lambda default=None, *a, **k: default,
             Body=lambda default=None, *a, **k: default,
             Header=lambda default=None, *a, **k: default,
             BackgroundTasks=object)
_stub_module("fastapi.responses", JSONResponse=_Response, PlainTextResponse=_Response,
             Response=_Response)
_stub_module("starlette.requests", Request=object)
_stub_module("starlette.responses", JSONResponse=_Response, Response=_Response)

def _permissive(mod):
    """Any name these stand-in modules are asked for resolves to something usable.

    The live router imports a long tail of pydantic/fastapi names (StrictBool,
    StrictInt, APIRouter, Query, ...). Enumerating them is a losing game; an
    attribute hook keeps the seam honest -- none of it is logic under test.
    """

    def _ga(attr):
        if attr.startswith("__"):
            raise AttributeError(attr)
        if attr in ("Field", "Query", "Body", "Header", "Path", "Cookie"):
            return _Field
        if "valid" in attr or attr in ("Depends", "depends"):
            return _decorator
        if attr[:1].isupper():
            return type(attr, (_BaseModel,), {})
        return _decorator

    mod.__getattr__ = _ga
    return mod


for _n in ("pydantic", "pydantic.fields", "pydantic.main", "pydantic_settings",
           "fastapi", "fastapi.responses"):
    if _n in sys.modules:
        _permissive(sys.modules[_n])

HTTPException = _HTTPException

_spec = importlib.util.spec_from_file_location(
    "hermes_cli.web_routers.oauth", QUARRY / "hermes_cli" / "web_routers" / "oauth.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

try:
    import hermes_cli.auth as hauth  # noqa: E402
except Exception:  # pragma: no cover
    hauth = _types.ModuleType("hermes_cli.auth")

WANTED = {
    "codex_http_error", "http_error_detail", "codex_start_error", "codex_user_code",
    "codex_exchange", "codex_cancelled", "gc_survivors", "device_start",
    "validate_profile",
}

STUB_TOKEN = "fixture-access-token"
STUB_REFRESH = "fixture-refresh-token"


class Resp:
    """Stand-in for an httpx response: status_code + json() + text."""

    def __init__(self, status=None, payload=None, text="", json_raises=False):
        if status is not None:
            self.status_code = status
        self._payload = payload
        self.text = text
        self._json_raises = json_raises

    def json(self):
        if self._json_raises:
            raise ValueError("no json body")
        return copy.deepcopy(self._payload)


def _resp_from(req):
    return Resp(status=req.get("status"), payload=req.get("payload"),
                text=req.get("text") or "", json_raises=bool(req.get("json_raises")))


def codex_http_error(req):
    cls = live._codex_http_error(req.get("status"))
    if cls is live._codex_err:
        return "codex_err"
    if cls is ConnectionError:
        return "ConnectionError"
    if cls is RuntimeError:
        return "RuntimeError"
    return getattr(cls, "__name__", "?")


def http_error_detail(req):
    return live._http_response_error_detail(_resp_from(req))


def codex_start_error(req):
    return live._codex_device_code_start_error(_resp_from(req))


def codex_user_code(req):
    device_data = req.get("device_data") or {}

    def fake_post(_httpx, _url, **_kwargs):
        return Resp(status=200, payload=device_data)

    with mock.patch.object(live, "_codex_post", fake_post):
        try:
            data = live._codex_request_user_code(None)
        except RuntimeError as exc:
            return {"interval": None, "ok": False, "error": str(exc)}
    return {"interval": data["interval"], "ok": True, "error": None}


def codex_exchange(req):
    captured = {}

    def fake_post(_httpx, _url, **kwargs):
        captured["data"] = kwargs.get("data")
        return Resp(status=200, payload={"access_token": STUB_TOKEN,
                                         "refresh_token": STUB_REFRESH})

    cid = req.get("client_id", "")
    with mock.patch.object(live, "_codex_post", fake_post), \
            mock.patch.object(hauth, "CODEX_OAUTH_CLIENT_ID", cid):
        try:
            live._codex_exchange_tokens(None, req.get("code_resp") or {})
        except RuntimeError as exc:
            return {"error": str(exc), "data": None}
    return {"error": None, "data": captured.get("data")}


def codex_cancelled(req):
    return bool(live._codex_cancelled(req.get("sess") or {}, "fixture-session", ""))


def gc_survivors(req):
    table = {}
    for sess in req.get("sessions") or []:
        sid = sess["session_id"]
        table[sid] = {"session_id": sid, "created_at": sess["created_at"]}
    with mock.patch.object(live, "_oauth_sessions", table), \
            mock.patch.object(live, "time") as fake_time:
        fake_time.time.return_value = req.get("now")
        live._gc_oauth_sessions()
    return list(table)


def device_start(req):
    table = {}
    started = []

    def fake_poller(target, sid, prefix="oauth-poll"):
        started.append(sid)

    with mock.patch.object(live, "_oauth_sessions", table), \
            mock.patch.object(live, "_start_poller", fake_poller):
        body = live._device_session_started(
            req["provider_id"], req.get("profile"), None, req.get("fields") or {},
            req["user_code"], req["verification_url"], req["expires_in"], req["poll_interval"])
        sid = body["session_id"]
        assert sid in table, "device session was not registered"
        sess = dict(table[sid])
    # both the id and the clock are seams: _new_oauth_session invents them
    sess["session_id"] = req["session_id"]
    sess["created_at"] = req["created_at"]
    body = dict(body)
    body["session_id"] = req["session_id"]
    return {"session": sess, "body": body}


def validate_profile(req):
    seen = []

    def fake_resolve(name):
        seen.append(name)

    with mock.patch.object(live, "_resolve_profile_dir", fake_resolve):
        name = live._validate_oauth_profile(req.get("profile"))
    return {"name": name, "resolved": bool(name)}


def expected(r):
    f = r.get("func")
    if f == "codex_http_error":
        return codex_http_error(r)
    if f == "http_error_detail":
        return http_error_detail(r)
    if f == "codex_start_error":
        return codex_start_error(r)
    if f == "codex_user_code":
        return codex_user_code(r)
    if f == "codex_exchange":
        return codex_exchange(r)
    if f == "codex_cancelled":
        return codex_cancelled(r)
    if f == "gc_survivors":
        return gc_survivors(r)
    if f == "device_start":
        return device_start(r)
    if f == "validate_profile":
        return validate_profile(r)
    raise AssertionError("unknown func %r" % (f,))


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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
