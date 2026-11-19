#!/usr/bin/env python3
"""Generate tests/fixtures/mcp_oauth_flow.jsonl for the mcp_oauth_flow oracle.

Each case: {"func": <name>, "in": {...}}. Scripted I/O lives in in.io
(seam-name -> {"ret": v} / {"raise": {...}} or a queue LIST of those). Object
returns MUST be wrapped in "ret". State (module globals / storage attrs) in
in.state. Run: python tests/gen_mcp_oauth_flow_cases.py
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "mcp_oauth_flow.jsonl"
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def ret(v):
    return {"ret": v}


def raise_(type_, message="", **kw):
    return {"raise": dict({"type": type_, "message": message}, **kw)}


def oserr(message="io error", errno=13):
    return raise_("OSError", message, errno=errno)


# ── refresh_lock_path ─────────────────────────────────────────────────────
for p in ["mcp-tokens/s.json", "tokens", ".hidden", "a.tar.gz", "dir.d/name",
          "a.", "dir/", "a/./b", "/abs/t.json", "a/../b.json"]:
    case("refresh_lock_path", path=p)

# ── acquire_refresh_fence ─────────────────────────────────────────────────
FENCE_OK = {"mkdir_under_hermes_home": ret(None), "secure_parent_dir": ret(None),
            "os.open": ret(7), "time.monotonic": ret(100.0), "fcntl.flock": ret(None)}
case("acquire_refresh_fence", path="t/s.json", timeout=60.0, platform="posix", io=dict(FENCE_OK))
case("acquire_refresh_fence", path="t/s.json", platform="posix",
     io={**FENCE_OK, "os.open": oserr("denied")})
case("acquire_refresh_fence", path="t/s.json", platform="posix",
     io={**FENCE_OK, "mkdir_under_hermes_home": oserr("no home", 2)})
case("acquire_refresh_fence", path="t/s.json", timeout=60.0, platform="posix",
     io={**FENCE_OK, "fcntl.flock": [oserr("locked", 11), ret(None)],
         "time.monotonic": [ret(100.0), ret(100.1)], "asyncio.sleep": ret(None)})
case("acquire_refresh_fence", path="t/s.json", timeout=60.0, platform="posix",
     io={**FENCE_OK, "fcntl.flock": [oserr("locked", 11), oserr("locked", 11)],
         "time.monotonic": [ret(100.0), ret(200.0)], "os.close": ret(None)})
case("acquire_refresh_fence", path="t/s.json", platform="posix",
     io={**FENCE_OK, "fcntl.flock": oserr("EIO", 5), "os.close": ret(None)})
case("acquire_refresh_fence", path="t/s.json", platform="windows",
     io={"mkdir_under_hermes_home": ret(None), "secure_parent_dir": ret(None),
         "os.open": ret(9), "time.monotonic": ret(100.0), "msvcrt.locking": ret(None)})
case("acquire_refresh_fence", path="t/s.json", platform="none",
     io={"mkdir_under_hermes_home": ret(None), "secure_parent_dir": ret(None),
         "os.open": ret(7), "time.monotonic": ret(100.0), "os.close": ret(None)})
case("acquire_refresh_fence", path="t/s.json", platform="posix",
     io={**FENCE_OK, "fcntl.flock": raise_("KeyboardInterrupt"), "os.close": ret(None)})
case("acquire_refresh_fence", path="t/s.json", timeout=60.0, platform="posix",
     io={**FENCE_OK, "fcntl.flock": [oserr("locked", 11), ret(None)],
         "time.monotonic": [ret(100.0), ret(100.1)],
         "asyncio.sleep": raise_("CancelledError"), "os.close": ret(None)})
case("acquire_refresh_fence", path="t/s.json", timeout=60.0, platform="posix",
     io={**FENCE_OK, "fcntl.flock": oserr("locked", 11),
         "time.monotonic": [ret(100.0), ret(200.0)], "os.close": oserr("bad fd", 9)})
case("acquire_refresh_fence", path="t/s.json", platform="posix",
     io={**FENCE_OK, "secure_parent_dir": oserr("insecure", 13)})
case("acquire_refresh_fence", path="t/s.json", timeout=60.0, platform="posix",
     io={**FENCE_OK, "fcntl.flock": [oserr("again", 11), oserr("deadlk", 35), ret(None)],
         "time.monotonic": [ret(100.0), ret(100.1), ret(100.2)], "asyncio.sleep": ret(None)})

# ── release_refresh_fence ─────────────────────────────────────────────────
case("release_refresh_fence", fd=7, platform="posix",
     io={"fcntl.flock": ret(None), "os.close": ret(None)})
case("release_refresh_fence", fd=7, platform="posix",
     io={"fcntl.flock": oserr("unlock fail"), "os.close": ret(None)})
case("release_refresh_fence", fd=7, platform="posix",
     io={"fcntl.flock": ret(None), "os.close": oserr("close fail", 9)})
case("release_refresh_fence", fd=7, platform="windows",
     io={"msvcrt.locking": ret(None), "os.close": ret(None)})
case("release_refresh_fence", fd=7, platform="posix",
     io={"fcntl.flock": raise_("KeyboardInterrupt"), "os.close": ret(None)})
case("release_refresh_fence", fd=7, platform="posix",
     io={"fcntl.flock": raise_("KeyboardInterrupt"), "os.close": oserr("x", 9)})
case("release_refresh_fence", fd=7, platform="none", io={"os.close": ret(None)})
case("release_refresh_fence", fd=7, platform="posix",
     io={"fcntl.flock": raise_("RuntimeError", "boom"), "os.close": raise_("KeyboardInterrupt")})

# ── sdk_class ─────────────────────────────────────────────────────────────
SHARED_FULL = {"OAuthClientInformationFull": "F", "OAuthClientMetadata": "M",
               "OAuthMetadata": "D", "OAuthToken": "T"}
EMPTY_CLASSES = {n: None for n in ["OAuthClientProvider", "OAuthClientInformationFull",
                                   "OAuthClientMetadata", "OAuthMetadata", "OAuthToken"]}
case("sdk_class", name="OAuthToken", state={"_SDK_CLASSES": {}, "_OAUTH_AVAILABLE": True},
     io={"import_client_auth": ret({"OAuthClientProvider": "P"}),
         "import_shared_auth": ret(SHARED_FULL)})
case("sdk_class", name="OAuthToken", state={"_SDK_CLASSES": {}, "_OAUTH_AVAILABLE": True},
     io={"import_client_auth": raise_("ImportError", "no sdk"), "logger.debug": ret(None)})
case("sdk_class", name="OAuthMetadata", state={"_SDK_CLASSES": {}, "_OAUTH_AVAILABLE": True},
     io={"import_client_auth": ret({"OAuthClientProvider": "P"}),
         "import_shared_auth": ret({"OAuthClientInformationFull": "F"}), "logger.debug": ret(None)})
case("sdk_class", name="OAuthToken",
     state={"_SDK_CLASSES": dict(EMPTY_CLASSES, OAuthToken="T"), "_OAUTH_AVAILABLE": True}, io={})
case("sdk_class", name="OAuthToken",
     state={"_SDK_CLASSES": dict(EMPTY_CLASSES), "_OAUTH_AVAILABLE": False}, io={})
case("sdk_class", name="OAuthClientProvider", state={"_SDK_CLASSES": {}, "_OAUTH_AVAILABLE": True},
     io={"import_client_auth": ret({}), "import_shared_auth": ret(SHARED_FULL), "logger.debug": ret(None)})
case("sdk_class", name="OAuthToken", state={"_SDK_CLASSES": {}, "_OAUTH_AVAILABLE": True},
     io={"import_client_auth": ret({"OAuthClientProvider": "P"}),
         "import_shared_auth": raise_("ImportError", "no shared"), "logger.debug": ret(None)})
case("sdk_class", name="Nope", state={"_SDK_CLASSES": {}, "_OAUTH_AVAILABLE": True},
     io={"import_client_auth": ret({"OAuthClientProvider": "P"}), "import_shared_auth": ret(SHARED_FULL)})
case("sdk_class", name="OAuthToken", state={"_SDK_CLASSES": {}, "_OAUTH_AVAILABLE": True},
     io={"import_client_auth": raise_("RuntimeError", "boom")})

# ── bind_reserved ─────────────────────────────────────────────────────────
BIND_OK = {"socket.socket": ret("s1"), "sock.bind": ret(None),
           "sock.getsockname": ret(["127.0.0.1", 54321])}
case("bind_reserved", port=0, state={"_reserved_sockets": {}}, io=dict(BIND_OK))
case("bind_reserved", port=8080, state={"_reserved_sockets": {}},
     io={**BIND_OK, "sock.bind": oserr("in use", 98), "sock.close": ret(None)})
case("bind_reserved", port=0, state={"_reserved_sockets": {}},
     io={**BIND_OK, "sock.bind": oserr("ephemeral fail", 98), "sock.close": ret(None)})
FULL8 = {str(10001 + i): chr(ord("a") + i) for i in range(8)}
case("bind_reserved", port=0, state={"_reserved_sockets": dict(FULL8)},
     io={**BIND_OK, "sock.getsockname": ret(["127.0.0.1", 20000]), "sock.close": ret(None)})
FULL8_CIMD = {"27890": "c", **{str(10001 + i): chr(ord("a") + i) for i in range(7)}}
case("bind_reserved", port=0, state={"_reserved_sockets": dict(FULL8_CIMD)},
     io={**BIND_OK, "sock.getsockname": ret(["127.0.0.1", 20001]), "sock.close": ret(None)})
case("bind_reserved", port=0, state={"_reserved_sockets": dict(FULL8)},
     io={**BIND_OK, "sock.getsockname": ret(["127.0.0.1", 20002]), "sock.close": oserr("close fail", 9)})
case("bind_reserved", port=0, state={"_reserved_sockets": {"54321": "old"}}, io=dict(BIND_OK))
case("bind_reserved", port=0, state={"_reserved_sockets": {}},
     io={**BIND_OK, "sock.bind": raise_("KeyboardInterrupt")})
case("bind_reserved", port=0, state={"_reserved_sockets": {}},
     io={**BIND_OK, "sock.getsockname": oserr("no name", 9)})

# ── cached_client_info ────────────────────────────────────────────────────
CI_PATH = {"storage._client_info_path": ret("/h/mcp-tokens/s.client.json")}
case("cached_client_info", storage=None, io={})
case("cached_client_info", storage={}, io={**CI_PATH, "_read_json": ret({"client_id": "c1"})})
case("cached_client_info", storage={}, io={**CI_PATH, "_read_json": ret([1, 2])})
case("cached_client_info", storage={}, io={**CI_PATH, "_read_json": ret(None)})
case("cached_client_info", storage={}, io={**CI_PATH, "_read_json": raise_("ValueError", "bad json")})
case("cached_client_info", storage={},
     io={"storage._client_info_path": raise_("AttributeError", "no path"), "_read_json": ret({})})
case("cached_client_info", storage={"broken": True}, io={})
case("cached_client_info", storage={}, io={**CI_PATH, "_read_json": raise_("RuntimeError", "boom")})
case("cached_client_info", storage={},
     io={"storage._client_info_path": raise_("TypeError", "bad"), "_read_json": ret({})})

# ── cached_redirect ───────────────────────────────────────────────────────
def cr(uris, storage=None):
    if storage is None:
        storage = {}
    io = {} if storage is None else {**CI_PATH, "_read_json": ret({"redirect_uris": uris})}
    case("cached_redirect", storage=storage, io=io)


cr(["https://proxy.example/cb", "http://127.0.0.1:8080/callback"])
cr(["http://localhost:99/callback", "https://p/cb"])
cr(["http://127.0.0.1:notaport/callback", "http://127.0.0.1:70/callback"])
cr(["http://h:99999/callback"])
cr([123, "http://127.0.0.1:80/callback"])
cr([None, "https://x/cb"])
cr(["https://", "http://localhost:5/callback"])
cr(["HTTP://USER@LOCALHOST:80/callback", "HTTPS://P/cb"])
cr(["http://[::1]:8080/callback"])
cr(["http://[::1/callback", "http://127.0.0.1:9/callback"])
cr(["http://127.0.0.1:80/other"])
case("cached_redirect", storage=None, io={})
case("cached_redirect", storage={}, io={**CI_PATH, "_read_json": ret([1, 2])})
case("cached_redirect", storage={}, io={**CI_PATH, "_read_json": ret({"redirect_uris": "notastring"})})
cr([True, "http://127.0.0.1:3/callback"])
cr(["http://127.0.0.1.:80/callback"])
cr(["http://127.0.0.1:80/callback?x=1#f"])
cr(["http://127.0.0.1:/callback"])
cr(["http://[abc]:80/callback", "http://localhost:4/callback"])
cr(["http://[1.2.3.4]:80/callback", "http://127.0.0.1:6/callback"])
cr(["https://x.com", "http://127.0.0.1:80/callback", "https://second/cb"])

# ── load_model ────────────────────────────────────────────────────────────
LM = {"_read_json": ret({"access_token": "x"}), "_sdk_class": ret("OAuthToken"),
      "model_validate": ret({"access_token": "x"})}
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens",
     io={"_read_json": ret(None)})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens",
     io={"_read_json": ret([1, 2]), "logger.warning": ret(None)})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens",
     io={"_read_json": ret({"a": 1}), "_sdk_class": ret(None)})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens", io=dict(LM))
case("load_model", path="/h/s.meta.json", sdk_name="OAuthMetadata", label="OAuth metadata",
     io={"_read_json": ret({"device_authorization_endpoint": "https://d"}),
         "_sdk_class": ret("OAuthMetadata"), "import_device_metadata": ret("DeviceOAuthMetadata"),
         "model_validate": ret({"ok": True})})
case("load_model", path="/h/s.meta.json", sdk_name="OAuthMetadata", label="OAuth metadata",
     io={"_read_json": ret({"device_authorization_endpoint": ""}),
         "_sdk_class": ret("OAuthMetadata"), "model_validate": ret({"ok": True})})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens", fixup=True,
     io={**LM, "fixup": ret(None)})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens", fixup=True,
     io={**LM, "fixup": raise_("ValueError", "bad fixup")})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens",
     io={**LM, "model_validate": raise_("ValueError", "invalid",
                                        errors=[{"loc": ["access_token"]}, {"loc": ["expires_in", 0]}]),
         "logger.warning": ret(None)})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens",
     io={**LM, "model_validate": raise_("KeyError", key="refresh_token"), "logger.warning": ret(None)})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens",
     io={**LM, "model_validate": raise_("RuntimeError", "boom")})
case("load_model", path="/h/s.meta.json", sdk_name="OAuthMetadata", label="OAuth metadata",
     io={"_read_json": ret({"device_authorization_endpoint": "https://d"}),
         "_sdk_class": ret("OAuthMetadata"), "import_device_metadata": raise_("ImportError", "no device")})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens",
     io={"_read_json": ret("hello"), "logger.warning": ret(None)})
case("load_model", path="/h/s.json", sdk_name="OAuthToken", label="tokens",
     io={"_read_json": ret(5), "logger.warning": ret(None)})

# ── rebase_expires_in ─────────────────────────────────────────────────────
REBASE_PATH = {"storage._tokens_path": ret("/h/s.json"), "path.stat": ret({"st_mtime": 1000.0})}
case("rebase_expires_in", data={"expires_at": 2000.0}, io={"time.time": ret(1000.0)})
case("rebase_expires_in", data={"expires_at": 500.0}, io={"time.time": ret(1000.0)})
case("rebase_expires_in", data={"expires_at": None, "expires_in": 3600},
     io={**REBASE_PATH, "time.time": ret(1500.0)})
case("rebase_expires_in", data={"expires_in": 3600},
     io={"storage._tokens_path": ret("/h/s.json"), "path.stat": oserr("no stat", 2)})
case("rebase_expires_in", data={"expires_in": "100"},
     io={**REBASE_PATH, "time.time": ret(1050.0)})
case("rebase_expires_in", data={"expires_in": None}, io={})
case("rebase_expires_in", data={"expires_at": "x"}, io={"time.time": ret(1000.0)})
case("rebase_expires_in", data={}, io={})
case("rebase_expires_in", data={"expires_in": [1]}, io=dict(REBASE_PATH))
case("rebase_expires_in", data={"expires_at": 2000.9}, io={"time.time": ret(1000.5)})
case("rebase_expires_in", data={"expires_in": True},
     io={**REBASE_PATH, "time.time": ret(1000.0)})
case("rebase_expires_in", data={"expires_in": 3600},
     io={"storage._tokens_path": ret("/h/s.json"), "path.stat": raise_("TypeError", "bad")})

# ── fixup_loaded_tokens ───────────────────────────────────────────────────
case("fixup_loaded_tokens", data={"hermes_issuer": "iss1", "expires_at": 2000.0},
     state={"loaded_issuer": None}, io={"time.time": ret(1000.0)})
case("fixup_loaded_tokens", data={"expires_at": 2000.0},
     state={"loaded_issuer": "prev"}, io={"time.time": ret(1000.0)})
case("fixup_loaded_tokens", data={"hermes_issuer": None}, state={"loaded_issuer": "prev"}, io={})
case("fixup_loaded_tokens", data={"hermes_issuer": "i", "expires_in": 100},
     state={"loaded_issuer": None}, io={**REBASE_PATH, "time.time": ret(1050.0)})
case("fixup_loaded_tokens", data={}, state={"loaded_issuer": "prev"}, io={})

# ── stamp_issuer ──────────────────────────────────────────────────────────
ST = {"storage._tokens_path": ret("/h/s.json")}
case("stamp_issuer", issuer="iss1", state={"_server_name": "s", "loaded_issuer": None},
     io={**ST, "_read_json": ret({"access_token": "x"}), "_write_json": ret(None)})
case("stamp_issuer", issuer="iss1", state={"_server_name": "s", "loaded_issuer": None},
     io={**ST, "_read_json": ret(None)})
case("stamp_issuer", issuer="new", state={"_server_name": "s", "loaded_issuer": None},
     io={**ST, "_read_json": ret({"hermes_issuer": "old"})})
case("stamp_issuer", issuer="new", state={"_server_name": "s", "loaded_issuer": None},
     io={**ST, "_read_json": ret({"hermes_issuer": ""}), "_write_json": ret(None)})
case("stamp_issuer", issuer="iss1", state={"_server_name": "s", "loaded_issuer": None},
     io={**ST, "_read_json": ret({"access_token": "x"}), "_write_json": oserr("disk full", 28),
         "logger.debug": ret(None)})
case("stamp_issuer", issuer="iss1", state={"_server_name": "s", "loaded_issuer": None},
     io={**ST, "_read_json": ret([1])})
case("stamp_issuer", issuer="iss1", state={"_server_name": "s", "loaded_issuer": None},
     io={"storage._tokens_path": [ret("/h/s.json"), oserr("path fail", 2)],
         "_read_json": ret({"access_token": "x"}), "logger.debug": ret(None)})
case("stamp_issuer", issuer="iss1", state={"_server_name": "s", "loaded_issuer": None},
     io={**ST, "_read_json": ret({"access_token": "x"}), "_write_json": raise_("RuntimeError", "boom")})

# ── strip_refresh_token ───────────────────────────────────────────────────
case("strip_refresh_token", state={"_server_name": "s", "loaded_issuer": "old"},
     io={**ST, "_read_json": ret({"refresh_token": "r", "hermes_issuer": "i", "access_token": "a"}),
         "_write_json": ret(None), "logger.info": ret(None)})
case("strip_refresh_token", state={"_server_name": "s", "loaded_issuer": "old"},
     io={**ST, "_read_json": ret(None)})
case("strip_refresh_token", state={"_server_name": "s", "loaded_issuer": "old"},
     io={**ST, "_read_json": ret({"access_token": "a"})})
case("strip_refresh_token", state={"_server_name": "s", "loaded_issuer": "old"},
     io={**ST, "_read_json": ret({"refresh_token": ""})})
case("strip_refresh_token", state={"_server_name": "s", "loaded_issuer": "old"},
     io={**ST, "_read_json": ret({"refresh_token": "r"}), "_write_json": oserr("disk", 28),
         "logger.warning": ret(None)})
case("strip_refresh_token", state={"_server_name": "s", "loaded_issuer": "old"},
     io={**ST, "_read_json": ret({"refresh_token": "r"}), "_write_json": ret(None), "logger.info": ret(None)})
case("strip_refresh_token", state={"_server_name": "s", "loaded_issuer": "old"},
     io={**ST, "_read_json": ret([1])})

# ── coerce_secret_auth_method ─────────────────────────────────────────────
for data in [{"client_secret": "s"}, {"client_secret": "s", "token_endpoint_auth_method": "none"},
             {"client_secret": "s", "token_endpoint_auth_method": ""},
             {"client_secret": "s", "token_endpoint_auth_method": "client_secret_basic"},
             {}, {"client_secret": ""}, {"client_secret": "s", "token_endpoint_auth_method": None},
             {"client_secret": "s", "token_endpoint_auth_method": "client_secret_post"},
             {"client_secret": 0}]:
    case("coerce_secret_auth_method", data=data)

# ── result_taken ──────────────────────────────────────────────────────────
for result in [{}, {"auth_code": ""}, {"auth_code": None, "error": None}, {"error": "x"},
               {"auth_code": 0}, {"error": False}, {"auth_code": "c", "state": "s"}]:
    case("result_taken", result=result)

# ── claim_result ──────────────────────────────────────────────────────────
EMPTY_RESULT = {"auth_code": None, "state": None, "error": None, "iss": None}
LOCK_OK = {"lock.__enter__": ret(None), "lock.__exit__": ret(None)}
case("claim_result", result=dict(EMPTY_RESULT), values={"auth_code": "c", "state": "s"}, io=dict(LOCK_OK))
case("claim_result", result=dict(EMPTY_RESULT, auth_code="existing"),
     values={"auth_code": "c"}, io=dict(LOCK_OK))
case("claim_result", result=dict(EMPTY_RESULT, error="e"), values={"auth_code": "c"}, io=dict(LOCK_OK))
case("claim_result", result=dict(EMPTY_RESULT), values={"error": "access_denied"}, io=dict(LOCK_OK))
case("claim_result", result={"auth_code": None, "error": None}, values={"auth_code": "c"}, io=dict(LOCK_OK))
case("claim_result", result=dict(EMPTY_RESULT), values={"auth_code": "c"},
     io={"lock.__enter__": raise_("RuntimeError", "x"), "lock.__exit__": ret(None)})
case("claim_result", result=dict(EMPTY_RESULT), values={"auth_code": "c"},
     io={"lock.__enter__": ret(None), "lock.__exit__": raise_("RuntimeError", "x")})

# ── start_callback_server ─────────────────────────────────────────────────
SRV = {"CallbackServer": ret({"server": "srv1", "socket": "s0"})}
case("start_callback_server", port=8080, handler="H", state={"_reserved_sockets": {}},
     io={**SRV, "server.server_bind": ret(None), "server.server_activate": ret(None)})
case("start_callback_server", port=8080, handler="H", state={"_reserved_sockets": {"8080": "parked"}},
     io={**SRV, "server.socket.close": ret(None), "reserved.getsockname": ret(["127.0.0.1", 8080]),
         "server.server_activate": ret(None)})
case("start_callback_server", port=8080, handler="H", state={"_reserved_sockets": {}},
     io={"CallbackServer": oserr("in use", 98)})
case("start_callback_server", port=8080, handler="H", state={"_reserved_sockets": {}},
     io={**SRV, "server.server_bind": oserr("bind fail", 98), "server.server_activate": ret(None)})
case("start_callback_server", port=8080, handler="H", state={"_reserved_sockets": {}},
     io={**SRV, "server.server_bind": ret(None), "server.server_activate": oserr("activate fail", 98)})
case("start_callback_server", port=8080, handler="H", state={"_reserved_sockets": {"8080": "p"}},
     io={**SRV, "server.socket.close": ret(None), "reserved.getsockname": oserr("no name", 9)})
case("start_callback_server", port=8080, handler="H", state={"_reserved_sockets": {"8080": "p"}},
     io={**SRV, "server.socket.close": oserr("close fail", 9), "reserved.getsockname": ret(["127.0.0.1", 8080])})
case("start_callback_server", port=8080, handler="H", state={"_reserved_sockets": {}},
     io={**SRV, "server.server_bind": raise_("KeyboardInterrupt"), "server.server_activate": ret(None)})
case("start_callback_server", port=9090, handler="H", state={"_reserved_sockets": {"9090": "p"}},
     io={**SRV, "server.socket.close": ret(None), "reserved.getsockname": ret(["127.0.0.1", 9090]),
         "server.server_activate": ret(None)})

# ── callback_outcome ──────────────────────────────────────────────────────
CIMD = "https://nousresearch.github.io/hermes-agent/docs/oauth/client-metadata.json"
case("callback_outcome", result={"error": "__hermes_user_skipped__", "auth_code": None, "state": None},
     cimd_url=None, io={})
case("callback_outcome", result={"error": "access_denied", "auth_code": None, "state": None},
     cimd_url=None, io={})
case("callback_outcome", result={"error": None, "auth_code": None, "state": None}, cimd_url=None, io={})
case("callback_outcome", result={"error": None, "auth_code": None, "state": None}, cimd_url=CIMD, io={})
case("callback_outcome", result={"error": None, "auth_code": "c", "state": "s", "iss": "i"},
     cimd_url=None, io={"authorization_code_result": ret(["c", "s"])})
case("callback_outcome", result={"error": "", "auth_code": "c", "state": "s"},
     cimd_url=None, io={"authorization_code_result": ret({"code": "c", "state": "s", "iss": None})})
case("callback_outcome", result={"error": None, "auth_code": "", "state": "s"},
     cimd_url=None, io={"authorization_code_result": ret(["", "s"])})
case("callback_outcome", result={"auth_code": "c"}, cimd_url=None, io={})
case("callback_outcome", result={"error": None, "auth_code": "c"}, cimd_url=None, io={})
case("callback_outcome", result={"error": 5, "auth_code": None}, cimd_url=None, io={})

# ── login_connect_timeout ─────────────────────────────────────────────────
for config in [{}, {"connect_timeout": 600}, {"oauth": {"timeout": 600}},
               {"connect_timeout": 100, "oauth": {"timeout": 600}},
               {"connect_timeout": "45"}, {"connect_timeout": None}, {"oauth": None},
               {"oauth": {"timeout": "abc"}}, {"connect_timeout": -5}, {"oauth": "x"},
               {"connect_timeout": True}, {"oauth": {"timeout": 0}},
               {"connect_timeout": "nan"}, {"oauth": {"timeout": "1e2"}},
               {"connect_timeout": " 45.5 "}, {"oauth": {}},
               {"connect_timeout": 400.5, "oauth": {"timeout": 100}}]:
    case("login_connect_timeout", config=config)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")
    assert all(v >= 5 for v in counts.values()), "each func needs >=5 cases"
    assert len(counts) == 18, f"expected 18 funcs, got {len(counts)}"


if __name__ == "__main__":
    main()
