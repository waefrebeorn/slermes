#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/oauth.py (gate w29_oauth_dc3549_p1).

The live router module is imported BY FILE PATH (it is not a package import here),
and its pure accounts-tab helpers are called on fixture values:

  * _truncate_token / _oauth_profile_name (web_server_oauth helpers it re-exports);
  * _epoch_ms_to_iso;
  * _status_card;
  * _resolve_provider_status + _PROVIDER_STATUS - the auth store getters are
    monkeypatched to return a canned raw dict (that is the seam: no store read);
  * _claude_code_disconnect_command / _oauth_provider_disconnect_command /
    _oauth_provider_disconnect_hint / _disconnect_http_error /
    _reject_if_not_disconnectable.

Fixture-only conventions
  {"__callable__": true} as a token value stands for a callable (Entra-ID bearer)
  provider so the "never invoked" branch is compared without a real callable.
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
GATE = "w29_oauth_dc3549_p1"

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
    "truncate_token", "epoch_ms_to_iso", "status_card", "resolve_provider_status",
    "claude_disconnect", "disconnect_command", "disconnect_hint",
    "disconnect_http_error", "reject_detail", "profile_name",
}

# The auth-store getters _PROVIDER_STATUS / the slug dispatcher call.
GETTERS = (
    "get_nous_auth_status_local", "get_codex_auth_status", "get_qwen_auth_status",
    "get_minimax_oauth_auth_status", "get_xai_oauth_auth_status", "get_auth_status",
)


def _token(value):
    if isinstance(value, dict) and value.get("__callable__"):
        return lambda: "never-called"
    return value


def truncate_token(req):
    return live._truncate_token(_token(req.get("value")), req.get("visible", 6))


def epoch_ms_to_iso(req):
    return live._epoch_ms_to_iso(req.get("value"))


def status_card(req):
    args = [
        req.get("raw"), req.get("source"), req.get("source_label"),
        req.get("token_preview"), req.get("expires_at"), req.get("has_refresh_token"),
    ]
    if req.get("last_refresh_present"):
        return live._status_card(*args, req.get("last_refresh"))
    return live._status_card(*args)


def resolve_provider_status(req):
    pid = req.get("provider_id")
    raw = req.get("raw")
    boom = req.get("raises")

    def getter(*_a, **_k):
        if boom:
            raise RuntimeError(boom)
        return copy.deepcopy(raw)

    patches = [mock.patch.object(hauth, g, getter) for g in GETTERS]
    for p in patches:
        p.start()
    try:
        if req.get("status_fn"):
            def status_fn():
                if boom:
                    raise RuntimeError(boom)
                return copy.deepcopy(raw)
            return live._resolve_provider_status(pid, status_fn)
        return live._resolve_provider_status(pid, None)
    finally:
        for p in patches:
            p.stop()


def claude_disconnect(req):
    return live._claude_code_disconnect_command(req.get("platform", "linux"))


def disconnect_command(req):
    return live._oauth_provider_disconnect_command(req.get("provider"), req.get("platform", "linux"))


def disconnect_hint(req):
    return live._oauth_provider_disconnect_hint(req.get("provider"), req.get("status"))


def disconnect_http_error(req):
    e = live._disconnect_http_error(req.get("status_code"), req.get("provider_name"))
    return {"status_code": e.status_code, "detail": e.detail}


def reject_detail(req):
    try:
        live._reject_if_not_disconnectable(req.get("provider"), req.get("status"))
    except HTTPException as exc:
        return exc.detail
    return None


def profile_name(req):
    return live._oauth_profile_name(req.get("profile"))


def expected(r):
    f = r.get("func")
    if f == "truncate_token":
        return truncate_token(r)
    if f == "epoch_ms_to_iso":
        return epoch_ms_to_iso(r)
    if f == "status_card":
        return status_card(r)
    if f == "resolve_provider_status":
        return resolve_provider_status(r)
    if f == "claude_disconnect":
        return claude_disconnect(r)
    if f == "disconnect_command":
        return disconnect_command(r)
    if f == "disconnect_hint":
        return disconnect_hint(r)
    if f == "disconnect_http_error":
        return disconnect_http_error(r)
    if f == "reject_detail":
        return reject_detail(r)
    if f == "profile_name":
        return profile_name(r)
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
