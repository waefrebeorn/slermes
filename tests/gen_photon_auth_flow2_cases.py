#!/usr/bin/env python3
"""Generate tests/fixtures/photon_auth_flow2.jsonl for the Photon auth FLOW port.

Case shape: {"func": <live name>, "in": {"args": {...}, "state": {},
"io": {seam-name: script}}}.  A script is a plain JSON value (constant), a
LIST consumed one element per call (wrap list RESULTS one level deeper), or
{"__script__": true, "ret": v} / {"__script__": true, "raise": {type,...}}.
Missing scripts are fail-closed RuntimeErrors on both sides (fc=True marks an
intentional gap).  Every ported function gets >= 5 cases; error paths, raise
propagation and the full builtin exception family are covered per flow.

Corpus restrictions mirror the C port header: str()/f-string slots carry
scalars only; json.load slots carry canonical JSON; E.164/phone text is ASCII;
auth dicts are dict-or-absent; getters return str/null; os.fdopen gets an int fd.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "photon_auth_flow2.jsonl")
CASES = []


def add(func, args=None, io=None, fc=False):
    c = {"func": func, "in": {"args": args or {}, "state": {}, "io": io or {}}}
    if fc:
        c["fc"] = True
    CASES.append(c)


def rs(kind, msg, **extra):
    r = {"type": kind, "message": msg}
    r.update(extra)
    return {"__script__": True, "raise": r}


def ret(v):
    return {"__script__": True, "ret": v}


# common seam bundles
def host(v=None):
    return {"os.getenv": v}


def getio(data=None, status=200, hostv=None):
    return {"os.getenv": hostv, "httpx.get": {"status_code": status},
            "resp.raise_for_status": None, "resp.json": data}


def postio(data=None, status=200, hostv=None, resp=None):
    return {"os.getenv": hostv, "httpx.post": resp or {"status_code": status},
            "resp.raise_for_status": None, "resp.json": data}


# ── _bearer ──
for tok in ("tok", "", "abc123", "a b", "x-y-z", None, 123):
    add("_bearer", {"token": tok})

# ── _basic ──
for pid, sec in (("p", "s"), ("", ""), ("id-1", "secret-2"), ("a:b", "c"),
                 (123, 456), (None, None)):
    add("_basic", {"project_id": pid, "project_secret": sec})

# ── _require_httpx ──
add("_require_httpx", {"httpx_available": True})
for w in ("", " device login", " user creation", " device login"):
    add("_require_httpx", {"httpx_available": False, "what": w})
add("_require_httpx", {"httpx_available": True, "what": "ignored"})

# ── _dashboard_host ──
for v in (None, "", "https://custom", "https://custom/", "https://a/b///", "/x"):
    add("_dashboard_host", {}, io=host(v))

# ── _spectrum_host ──
for v in (None, "", "https://spec", "https://spec/", "https://s//", "//"):
    add("_spectrum_host", {}, io=host(v))

# ── _clean_bearer_token ──
for v in ("tok", "  tok  ", "Bearer abc", "bearer   xyz ", "BEARER t", "",
          None, 123, "   "):
    add("_clean_bearer_token", {"value": v})

# ── _header_value ──
add("_header_value", {"headers": {"set-auth-token": "abc"}, "name": "set-auth-token"})
add("_header_value", {"headers": {"Set-Auth-Token": "xyz"}, "name": "set-auth-token"})
add("_header_value", {"headers": {}, "name": "set-auth-token"})
add("_header_value", {"headers": None, "name": "set-auth-token"})
add("_header_value", {"headers": {"other": "v"}, "name": "set-auth-token"})
add("_header_value", {"headers": {"set-auth-token": 123}, "name": "set-auth-token"})

# ── _normalize_phone ──
for p in ("+1 (555) 123-4567", "+15551234567", "abc+1x2", "", None, "+1.555.000.0000"):
    add("_normalize_phone", {"phone": p})

# ── _raise_on_error_key ──
for data, action in (({}, "x"), ({"error": "bad"}, "create-project"),
                     ({"error": ""}, "x"), ({"error": 123}, "y"),
                     ({"message": "ignored"}, "z"), (None, "q")):
    add("_raise_on_error_key", {"data": data, "action": action})

# ── _unwrap_list ──
for d in ([1, 2], {"data": [1]}, {"projects": [{"id": "p"}]},
          {"data": {"projects": [1, 2]}}, {"x": 1}, None, {}):
    add("_unwrap_list", {"data": d})

# ── user_assigned_line ──
for u in ({"assignedPhoneNumber": "+1555"}, {"assignedPhoneNumber": ""},
          {"assignedPhoneNumber": 123}, {}, None, {"phoneNumber": "+1"}):
    add("user_assigned_line", {"user": u})

# ── user_opted_in ──
for u in ({"meta": {"opt_in": True}}, {"meta": {"opt_in": False}},
          {"meta": {}}, {"meta": 1}, {}, None, {"meta": {"opt_in": "yes"}}):
    add("user_opted_in", {"user": u})

# ── _get_config_env_value ──
add("_get_config_env_value", {"key": "K"}, io={"config.get_env_value": "v"})
add("_get_config_env_value", {"key": "K", "config_available": False}, io={"os.getenv": "env"})
add("_get_config_env_value", {"key": "K"}, io={"config.get_env_value": None})
add("_get_config_env_value", {"key": "K"}, io={"config.get_env_value": rs("RuntimeError", "x")})
add("_get_config_env_value", {"key": "K", "config_available": False}, io={"os.getenv": None})
add("_get_config_env_value", {"key": "K", "config_available": False},
    io={"os.getenv": rs("OSError", "no")})

# ── _configured_operator_phone ──
add("_configured_operator_phone", {}, io={"config.get_env_value": ["+15551234567", ""]})
add("_configured_operator_phone", {}, io={"config.get_env_value": ["", "+15550000000"]})
add("_configured_operator_phone", {}, io={"config.get_env_value": ["", ""]})
add("_configured_operator_phone", {}, io={"config.get_env_value": ["", "+111, +222"]})
add("_configured_operator_phone", {}, io={"config.get_env_value": ["", "+111 +222"]})
add("_configured_operator_phone", {}, io={"config.get_env_value": ["", "junk"]})
add("_configured_operator_phone", {"config_available": False}, io={"os.getenv": ["", "+15551234567"]})

# ── _safe ──
add("_safe", {"fn": "ok"}, io={"safe_call": None})
add("_safe", {"fn": "ok"}, io={"safe_call": rs("RuntimeError", "x")})
add("_safe", {"fn": "ok"}, io={"safe_call": rs("OSError", "x")})
add("_safe", {"fn": "ok"}, io={"safe_call": rs("SystemExit", "bye")})
add("_safe", {"fn": "ok"}, io={"safe_call": rs("ValueError", "x")})
add("_safe", {"fn": "b"}, io={"safe_call": rs("KeyboardInterrupt", "stop")})

# ── _response_error_detail ──
add("_response_error_detail", {"resp": {"text": "plain"}}, io={"resp.json": rs("ValueError", "bad")})
add("_response_error_detail", {"resp": {"text": "plain"}}, io={"resp.json": {"error": "boom"}})
add("_response_error_detail", {"resp": {"text": "plain"}}, io={"resp.json": {"message": "m"}})
add("_response_error_detail", {"resp": {"text": "plain"}}, io={"resp.json": {"detail": "d"}})
add("_response_error_detail", {"resp": {"text": "plain"}}, io={"resp.json": {"a": 1, "b": 2}})
add("_response_error_detail", {"resp": {"text": ""}}, io={"resp.json": None})
add("_response_error_detail", {"resp": {"text": "x" * 600}}, io={"resp.json": rs("TypeError", "bad")})

# ── _raise_for_status ──
S200 = {"status_code": 200, "text": "ok"}
add("_raise_for_status", {"resp": S200, "action": "x"}, io={"resp.json": None})
for sc in (400, 401, 403, 500):
    add("_raise_for_status", {"resp": {"status_code": sc, "text": "bad"}, "action": "list-users"},
        io={"resp.json": {"error": "denied"}})
add("_raise_for_status", {"resp": {"status_code": 503, "text": ""}, "action": "op"}, io={})
add("_raise_for_status", {"resp": {"status_code": 500, "text": "t"}, "action": "op"},
    io={"resp.json": rs("SystemExit", "bye")})

# ── _pool_first ──
add("_pool_first", {"auth": {"credential_pool": {"photon": [{"access_token": "t"}]}}, "key": "photon"})
add("_pool_first", {"auth": {"credential_pool": {}}, "key": "photon"})
add("_pool_first", {"auth": {}, "key": "photon"})
add("_pool_first", {"auth": {"credential_pool": {"photon": []}}, "key": "photon"})
add("_pool_first", {"auth": {"credential_pool": {"photon": "notlist"}}, "key": "photon"})
add("_pool_first", {"auth": {"credential_pool": {"photon": [1, 2]}}, "key": "photon"})

# ── _auth_json_path ──
for h in ("/h", "/", "//", "/a/b", "h", "/h/"):
    add("_auth_json_path", {}, io={"get_hermes_home": h})
add("_auth_json_path", {}, io={"get_hermes_home": rs("RuntimeError", "x")})

# ── _load_auth ──
LA = {"get_hermes_home": "/h", "path.exists": True, "json.load": {"a": 1}}
add("_load_auth", {}, io=LA)
add("_load_auth", {}, io={"get_hermes_home": "/h", "path.exists": False})
add("_load_auth", {}, io={"get_hermes_home": "/h", "path.exists": True, "json.load": None})
add("_load_auth", {}, io={"get_hermes_home": "/h", "path.exists": True,
                          "json.load": rs("OSError", "no")})
add("_load_auth", {}, io={"get_hermes_home": "/h", "path.exists": True,
                          "json.load": rs("JSONDecodeError", "bad")})
add("_load_auth", {}, io={"get_hermes_home": "/h", "path.exists": True,
                          "json.load": rs("RuntimeError", "boom")})
add("_load_auth", {}, io={"get_hermes_home": rs("SystemExit", "bye")})

# ── _save_auth ──
SAVE_OK = {"get_hermes_home": "/h", "path.mkdir": None, "os.getpid": 123,
           "uuid4_hex": "abc", "os.open": 7, "os.fdopen": "fh", "json.dump": None,
           "os.fsync": None, "path.replace": None}
add("_save_auth", {"data": {"a": 1}}, io=dict(SAVE_OK))
add("_save_auth", {"data": {}}, io={k: (rs("OSError", "no") if k == "path.mkdir" else v)
                                     for k, v in SAVE_OK.items()})
add("_save_auth", {"data": {}}, io={k: (rs("RuntimeError", "x") if k == "os.getpid" else v)
                                     for k, v in SAVE_OK.items()})
add("_save_auth", {"data": {}}, io={k: (rs("OSError", "x") if k == "os.open" else v)
                                     for k, v in SAVE_OK.items()})
add("_save_auth", {"data": {}}, io={k: (rs("RuntimeError", "x") if k == "os.fdopen" else v)
                                     for k, v in SAVE_OK.items()})
add("_save_auth", {"data": {}}, io={k: (rs("RuntimeError", "x") if k == "json.dump" else v)
                                     for k, v in SAVE_OK.items()})
add("_save_auth", {"data": {}}, io={k: (rs("OSError", "x") if k == "os.fsync" else v)
                                     for k, v in SAVE_OK.items()})
add("_save_auth", {"data": {}}, io={k: (rs("RuntimeError", "x") if k == "path.replace" else v)
                                     for k, v in SAVE_OK.items()})
add("_save_auth", {"data": {}}, io={k: (rs("SystemExit", "bye") if k == "get_hermes_home" else v)
                                     for k, v in SAVE_OK.items()})

# ── _store_pool_record ──
SPR = {"auth_store_lock": None, "get_hermes_home": "/h", "path.exists": True,
       "json.load": {"credential_pool": {"photon": []}}, "path.mkdir": None,
       "os.getpid": 1, "uuid4_hex": "h", "os.open": 3, "os.fdopen": "fh",
       "json.dump": None, "os.fsync": None, "path.replace": None}
add("_store_pool_record", {"key": "photon", "record": {"access_token": "t"}}, io=dict(SPR))
add("_store_pool_record", {"key": "photon", "record": {"x": 1}},
    io={k: (rs("RuntimeError", "x") if k == "auth_store_lock" else v) for k, v in SPR.items()})
add("_store_pool_record", {"key": "photon", "record": {}},
    io={k: (rs("OSError", "x") if k == "path.replace" else v) for k, v in SPR.items()})
add("_store_pool_record", {"key": "photon_project", "record": {"id": "p"}},
    io={k: ({"credential_pool": {}} if k == "json.load" else v) for k, v in SPR.items()})
add("_store_pool_record", {"key": "photon", "record": {}}, fc=True)

# ── load_photon_token ──
def LPT(auth):
    return {"get_hermes_home": "/h", "path.exists": True, "json.load": auth}


add("load_photon_token", {}, io=LPT({"credential_pool": {"photon": [{"access_token": "t"}]}}))
add("load_photon_token", {}, io=LPT({"credential_pool": {"photon": [{"token": "legacy"}]}}))
add("load_photon_token", {}, io=LPT({"providers": {"photon": {"access_token": "old"}}}))
add("load_photon_token", {}, io=LPT({}))
add("load_photon_token", {}, io=LPT({"credential_pool": {"photon": []}}))
add("load_photon_token", {}, io=LPT({"credential_pool": {"photon": [{"access_token": 123}]}}))

# ── store_photon_token ──
add("store_photon_token", {"token": "t"}, io={"time.time": 100.0, **SPR})
add("store_photon_token", {"token": "x"}, io={"time.time": rs("RuntimeError", "x")})
add("store_photon_token", {"token": ""}, io={"time.time": 0, **SPR})
add("store_photon_token", {"token": "t"},
    io={k: (rs("OSError", "x") if k == "path.replace" else v) for k, v in {"time.time": 1.0, **SPR}.items()})
add("store_photon_token", {"token": "t"}, io={})

# ── clear_photon_token ──
add("clear_photon_token", {}, io=LPT({"credential_pool": {"photon": [{"access_token": "t"}]},
                                       "providers": {"photon": {"x": 1}}}) | SAVE_OK)
add("clear_photon_token", {}, io=LPT({"credential_pool": {"photon": []}}) | SAVE_OK)
add("clear_photon_token", {}, io=LPT({}) | SAVE_OK)
add("clear_photon_token", {}, io=LPT({"providers": {"photon": {"x": 1}}}) | SAVE_OK)
add("clear_photon_token", {}, io=LPT({"credential_pool": "notobj"}) | SAVE_OK)
add("clear_photon_token", {}, io={"get_hermes_home": rs("SystemExit", "bye")})

# ── load_project_credentials ──
def LPC(auth):
    return {"get_scoped_secret": [None, None], "get_hermes_home": "/h",
            "path.exists": True, "json.load": auth}


add("load_project_credentials", {}, io=LPC({"credential_pool": {"photon_project": [
    {"spectrum_project_id": "sid", "project_secret": "sec"}]}}))
add("load_project_credentials", {}, io=LPC({"credential_pool": {"photon_project": [
    {"project_id": "old", "project_secret": "sec"}]}}))
add("load_project_credentials", {}, io={**LPC({}), "get_scoped_secret": ["id", "sec"]})
add("load_project_credentials", {}, io=LPC({}))
add("load_project_credentials", {}, io={**LPC({}), "get_scoped_secret": [rs("RuntimeError", "x"), None]})
add("load_project_credentials", {}, io={**LPC({}), "get_scoped_secret": [None, rs("SystemExit", "bye")]})

# ── load_dashboard_project_id ──
def LDPI(auth):
    return {"get_scoped_secret": None, "get_hermes_home": "/h",
            "path.exists": True, "json.load": auth}


for entry in ({"spectrum_project_id": "sid"}, {"dashboard_project_id": "d"},
              {"project_id": "p"}, {}, {"spectrum_project_id": "", "project_id": "p"}):
    add("load_dashboard_project_id", {}, io=LDPI({"credential_pool": {"photon_project": [entry]}}))
add("load_dashboard_project_id", {}, io={**LDPI({}), "get_scoped_secret": "env"})
add("load_dashboard_project_id", {}, io={**LDPI({}), "get_scoped_secret": rs("RuntimeError", "x")})

# ── store_project_credentials ──
SPC = {"time.time": 100.0, **SPR, "config.save_env_value": None}
add("store_project_credentials", {"spectrum_project_id": "p", "project_secret": "s"}, io=dict(SPC))
add("store_project_credentials", {"spectrum_project_id": "p", "project_secret": "s",
                                  "dashboard_project_id": "d", "name": "N"}, io=dict(SPC))
add("store_project_credentials", {"spectrum_project_id": "p", "project_secret": "s"},
    io={k: (rs("RuntimeError", "x") if k == "time.time" else v) for k, v in SPC.items()})
add("store_project_credentials", {"spectrum_project_id": "p", "project_secret": "s"},
    io={k: (rs("RuntimeError", "x") if k == "config.save_env_value" else v) for k, v in SPC.items()})
add("store_project_credentials", {"spectrum_project_id": "p", "project_secret": "s",
                                  "config_available": False}, io=dict(SPC))
add("store_project_credentials", {"spectrum_project_id": "p", "project_secret": "s"},
    io={k: (rs("SystemExit", "bye") if k == "config.save_env_value" else v) for k, v in SPC.items()})

# ── store_user_numbers ──
SUN = {"time.time": 100.0, **SPR}
add("store_user_numbers", {"phone_number": "+1555"}, io=dict(SUN))
add("store_user_numbers", {"assigned_phone_number": "+1666", "user_id": "u"}, io=dict(SUN))
add("store_user_numbers", {}, io={})
add("store_user_numbers", {"phone_number": "p", "dashboard_project_id": "d"}, io=dict(SUN))
add("store_user_numbers", {"phone_number": "p"},
    io={k: (rs("RuntimeError", "x") if k == "time.time" else v) for k, v in SUN.items()})
add("store_user_numbers", {"assigned_phone_number": "a"},
    io={k: (rs("OSError", "x") if k == "path.replace" else v) for k, v in SUN.items()})

# ── _persist_runtime_env ──
add("_persist_runtime_env", {"spectrum_project_id": "p", "project_secret": "s"},
    io={"config.save_env_value": None})
add("_persist_runtime_env", {"spectrum_project_id": "p", "project_secret": "s"},
    io={"config.save_env_value": rs("RuntimeError", "x")})
add("_persist_runtime_env", {"spectrum_project_id": "p", "project_secret": "s"},
    io={"config.save_env_value": [None, rs("OSError", "x")]})
add("_persist_runtime_env", {"spectrum_project_id": "p", "project_secret": "s"},
    io={"config.save_env_value": [None, rs("SystemExit", "bye")]})
add("_persist_runtime_env", {"spectrum_project_id": "p", "project_secret": "s"},
    io={"config.save_env_value": rs("SystemExit", "bye")})
add("_persist_runtime_env", {"spectrum_project_id": "p", "project_secret": "s",
                             "config_available": False}, io={})

# ── _device_response_token_candidates ──
add("_device_response_token_candidates", {"body": {"access_token": "t"}})
add("_device_response_token_candidates", {"body": {"access_token": "t", "accessToken": "t"}})
add("_device_response_token_candidates", {"body": {"data": {"access_token": "d"}}})
add("_device_response_token_candidates", {"body": {"session": {"access_token": "s"}}})
add("_device_response_token_candidates",
    {"body": {}, "headers": {"set-auth-token": "Bearer hdr"}})
add("_device_response_token_candidates", {"body": {"access_token": "Bearer xyz"}})
add("_device_response_token_candidates", {"body": {"access_token": 123}})

# ── request_device_code ──
add("request_device_code", {}, io=postio({"device_code": "d", "user_code": "u",
    "verification_uri": "https://v", "expires_in": 600, "interval": 3}))
add("request_device_code", {"scope": None}, io=postio({"device_code": "d", "user_code": "u",
    "verification_uri": "https://v"}))
add("request_device_code", {"client_id": "custom"}, io=postio({"device_code": "d", "user_code": "u",
    "verification_uri": "https://v", "verification_uri_complete": "https://v?c=1"}))
add("request_device_code", {}, io={"os.getenv": None,
    "httpx.post": {"status_code": 400}, "resp.raise_for_status": rs("RuntimeError", "bad")})
add("request_device_code", {}, io={"os.getenv": None, "httpx.post": {"status_code": 200},
    "resp.raise_for_status": None, "resp.json": rs("SystemExit", "bye")})
add("request_device_code", {"httpx_available": False}, io={})
add("request_device_code", {}, io=postio({"device_code": "d", "user_code": "u",
    "verification_uri": "https://v", "expires_in": "1800", "interval": "5"}))

# ── check_photon_token_valid ──
def cvio(status=200, user=None):
    return getio({"user": user} if user is not None else {}, status=status)


add("check_photon_token_valid", {"token": ""}, io={})
add("check_photon_token_valid", {"token": "t"}, io={"os.getenv": None,
    "httpx.get": {"status_code": 401}, "resp.raise_for_status": None, "resp.json": {}})
add("check_photon_token_valid", {"token": "t"}, io=cvio(200, {"id": "u"}))
add("check_photon_token_valid", {"token": "t"}, io=getio({}, status=500))
add("check_photon_token_valid", {"token": "t"}, io=cvio(200, {}))
add("check_photon_token_valid", {"token": "t"},
    io={"os.getenv": None, "httpx.get": rs("SystemExit", "bye")})

# ── validate_photon_token ──
add("validate_photon_token", {"token": "t"}, io={"os.getenv": None,
    "httpx.get": {"status_code": 200}, "resp.raise_for_status": [None, None],
    "resp.json": {"user": {"id": "u"}}})
add("validate_photon_token", {"token": "t"}, io={"os.getenv": None,
    "httpx.get": [{"status_code": 401}, {"status_code": 200}], "resp.json": {}})
add("validate_photon_token", {"token": "t"}, io={"os.getenv": None,
    "httpx.get": [{"status_code": 200}, {"status_code": 403}],
    "resp.raise_for_status": [None, None], "resp.json": {"user": {"id": "u"}}})
add("validate_photon_token", {"token": "t"}, io={"os.getenv": None,
    "httpx.get": {"status_code": 200}, "resp.raise_for_status": None, "resp.json": {}})
add("validate_photon_token", {"token": "t"}, io={"os.getenv": None,
    "httpx.get": {"status_code": 200}, "resp.raise_for_status": None,
    "resp.json": {"user": 1}})
add("validate_photon_token", {"token": "t"}, io={"os.getenv": None,
    "httpx.get": {"status_code": 500}, "resp.raise_for_status": rs("RuntimeError", "bad")})
add("validate_photon_token", {"token": "t"}, io={"os.getenv": None,
    "httpx.get": rs("SystemExit", "bye")})

# ── _validated_dashboard_token ──
def vdt(cands, ok=True):
    io = {"os.getenv": None, "httpx.get": {"status_code": 200},
          "resp.raise_for_status": [None, None], "resp.json": {"user": {"id": "u"}}}
    return io


C1 = [{"source": "poll", "token": "t"}]
add("_validated_dashboard_token", {"candidates": []}, io={})
add("_validated_dashboard_token", {"candidates": C1}, io=vdt(C1))
add("_validated_dashboard_token",
    {"candidates": [{"source": "a", "token": "a"}, {"source": "b", "token": "b"}]},
    io={"os.getenv": None, "httpx.get": {"status_code": 401}, "resp.json": {}})
add("_validated_dashboard_token", {"candidates": C1}, io={"os.getenv": None,
    "httpx.get": rs("SystemExit", "bye")})
add("_validated_dashboard_token", {"candidates": C1}, io={"os.getenv": None,
    "httpx.get": {"status_code": 200}, "resp.raise_for_status": None,
    "resp.json": {"user": {}}})
add("_validated_dashboard_token", {"candidates": C1}, io={"os.getenv": None,
    "httpx.get": {"status_code": 500}, "resp.raise_for_status": rs("RuntimeError", "bad")})

# ── _dashboard_get ──
add("_dashboard_get", {"path": "/api/x", "token": "t"}, io=host(None))
add("_dashboard_get", {"path": "/x", "token": "t", "what": " custom"}, io=host(None))
add("_dashboard_get", {"path": "/x", "token": "t"}, io=host(rs("RuntimeError", "x")))
add("_dashboard_get", {"path": "/x", "token": "t", "httpx_available": False}, io={})
add("_dashboard_get", {"path": "/x", "token": "t"},
    io={**host("https://custom"), "httpx.get": rs("SystemExit", "bye")})
add("_dashboard_get", {"path": "/x", "token": "t"}, io=host("https://h///"))

# ── _dashboard_post ──
add("_dashboard_post", {"path": "/x", "body": {"a": 1}, "token": "t"}, io=postio({"ok": 1}))
add("_dashboard_post", {"path": "/x", "body": {}, "token": "t"}, io=postio(None))
add("_dashboard_post", {"path": "/x", "body": {}, "token": "t", "what": " custom"}, io=postio({}))
add("_dashboard_post", {"path": "/x", "body": {}, "token": "t"},
    io={"os.getenv": None, "httpx.post": rs("RuntimeError", "x")})
add("_dashboard_post", {"path": "/x", "body": {}, "token": "t"},
    io={"os.getenv": None, "httpx.post": {"status_code": 200},
        "resp.raise_for_status": rs("SystemExit", "bye")})
add("_dashboard_post", {"path": "/x", "body": {}, "token": "t", "httpx_available": False}, io={})

# ── _dashboard_list ──
add("_dashboard_list", {"path": "/api/projects", "token": "t"}, io=getio([{"id": "p"}]))
add("_dashboard_list", {"path": "/x", "token": "t"}, io=getio({"data": [1, 2]}))
add("_dashboard_list", {"path": "/x", "token": "t"}, io=getio({}))
add("_dashboard_list", {"path": "/x", "token": "t"},
    io={"os.getenv": None, "httpx.get": {"status_code": 500},
        "resp.raise_for_status": rs("RuntimeError", "bad")})
add("_dashboard_list", {"path": "/x", "token": "t"},
    io={"os.getenv": None, "httpx.get": {"status_code": 200},
        "resp.raise_for_status": None, "resp.json": rs("SystemExit", "bye")})
add("_dashboard_list", {"path": "/x", "token": "t"}, io={"os.getenv": rs("OSError", "x")})

# ── list_projects ──
add("list_projects", {"token": "t"}, io=getio([{"id": "p", "name": "A"}]))
add("list_projects", {"token": "t"}, io=getio({"projects": [{"id": "q"}]}))
add("list_projects", {"token": "t"}, io=getio([]))
add("list_projects", {"token": "t"}, io=getio(None))
add("list_projects", {"token": "t"},
    io={"os.getenv": None, "httpx.get": rs("RuntimeError", "x")})
add("list_projects", {"token": "t", "httpx_available": False}, io={})

# ── find_project_by_name ──
def fpb(projects, name="Hermes Agent"):
    return getio(projects)


add("find_project_by_name", {"token": "t", "name": "Hermes Agent"},
    io=fpb([{"id": "p", "name": "hermes agent"}], "Hermes Agent"))
add("find_project_by_name", {"token": "t", "name": "nope"},
    io=fpb([{"id": "p", "name": "other"}], "nope"))
add("find_project_by_name", {"token": "t", "name": "  A  "},
    io=fpb([{"id": "p", "name": " a "}], "  A  "))
add("find_project_by_name", {"token": "t", "name": "a"}, io=fpb([], "a"))
add("find_project_by_name", {"token": "t", "name": "a"},
    io={"os.getenv": None, "httpx.get": rs("RuntimeError", "x")})
add("find_project_by_name", {"token": "t", "name": None}, io=fpb([{"name": None}], None))


# ── write ──
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"wrote {len(CASES)} cases to {OUT}")
for fn in sorted(counts):
    print(f"  {fn}: {counts[fn]}")
short = [fn for fn, n in counts.items() if n < 5]
if short:
    print(f"WARNING funcs with <5 cases: {short}")
