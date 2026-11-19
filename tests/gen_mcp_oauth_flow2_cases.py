#!/usr/bin/env python3
"""Generate tests/fixtures/mcp_oauth_flow2.jsonl for the mcp_oauth_flow2 oracle.

Each case: {"func": <name>, "in": {...}}. Scripted I/O lives in in.io
(seam-name -> {"ret": v} / {"raise": {...}} or a queue LIST of those). Object
returns MUST be wrapped in "ret". Storage attrs in in.state. >=5 cases per
ported function; BaseException passthrough and fail-closed branches covered.
Run: python tests/gen_mcp_oauth_flow2_cases.py
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "mcp_oauth_flow2.jsonl"
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def ret(v):
    return {"ret": v}


def raise_(type_, message="", **kw):
    return {"raise": dict({"type": type_, "message": message}, **kw)}


def oserr(message="io error", errno=13):
    return raise_("OSError", message, errno=errno)


# ── seconds (login_connect_timeout._seconds) ──────────────────────────────
case("seconds", value=5, default=0.0)
case("seconds", value="12.5", default=0.0)
case("seconds", value=-3, default=0.0)
case("seconds", value=None, default=300.0)
case("seconds", value="abc", default=7.5)
case("seconds", value=[1], default=0.0)
case("seconds", value="1_000.5", default=0.0)
case("seconds", value=True, default=0.0)
case("seconds", value="", default=2.5)
case("seconds", value={"a": 1}, default=9.0)
case("seconds", value="  +1_0.5e+2  ", default=0.0)
case("seconds", value=2.5, default=0.0)
case("seconds", value="1e", default=4.5)

# ── parse_redirect_query ──────────────────────────────────────────────────
for q in ["code=c1&state=s1&error=e1&iss=i1", "", "code=c1", "code=&state=s1",
          "code=a&code=b", "error=access_denied", "iss=https://issuer.example",
          "state=s%20t&code=c+d", "foo=bar", "code=%41%42", "a=1;b=2",
          "code=c&state=&error=&iss="]:
    case("parse_redirect_query", query=q)

# ── model_json ────────────────────────────────────────────────────────────
case("model_json", model="M", io={"model_dump": ret({"access_token": "***", "expires_in": 3600})})
case("model_json", model="M", io={"model_dump": ret(None)})
case("model_json", model="M", io={"model_dump": raise_("ValueError", "bad model")})
case("model_json", model="M", io={"model_dump": raise_("KeyboardInterrupt")})
case("model_json", model=None, io={})
case("model_json", model={}, io={})
case("model_json", model=2.5, io={})
case("model_json", model="M", io={"model_dump": oserr("dump fail")})

# ── path (HermesTokenStorage._path) ───────────────────────────────────────
PATH_STATE = {"_server_name": "s", "_hermes_home": "/h"}
GTD = {"get_token_dir": ret("/h/mcp-tokens")}
case("path", suffix=".json", state=dict(PATH_STATE), io=dict(GTD))
case("path", suffix=".client.json", state=dict(PATH_STATE), io=dict(GTD))
case("path", suffix=".meta.json", state={"_server_name": "s", "_hermes_home": None}, io=dict(GTD))
case("path", suffix=".json", state={"_hermes_home": "/h"}, io=dict(GTD))
case("path", suffix=".json", state={"_server_name": "s"}, io={})
case("path", suffix=".json", state=dict(PATH_STATE), io={"get_token_dir": oserr("no dir")})
case("path", suffix=".json", state=dict(PATH_STATE), io={"get_token_dir": ret(None)})
case("path", suffix=".cimd-off", state={"_server_name": "a b", "_hermes_home": "/h"}, io=dict(GTD))
case("path", suffix=".json", state=dict(PATH_STATE), io={"get_token_dir": raise_("KeyboardInterrupt")})

# ── state_paths ───────────────────────────────────────────────────────────
SP_OK = {"storage._tokens_path": ret("/h/mcp-tokens/s.json"),
         "storage._client_info_path": ret("/h/mcp-tokens/s.client.json"),
         "storage._meta_path": ret("/h/mcp-tokens/s.meta.json")}
case("state_paths", io=dict(SP_OK))
case("state_paths", io={**SP_OK, "storage._tokens_path": raise_("ValueError", "bad")})
case("state_paths", io={**SP_OK, "storage._client_info_path": raise_("KeyboardInterrupt")})
case("state_paths", io={**SP_OK, "storage._meta_path": ret(None)})
case("state_paths", io={"storage._tokens_path": [ret("/t.json")],
                        "storage._client_info_path": [ret("/c.json")],
                        "storage._meta_path": [ret("/m.json")]})
case("state_paths", io={"storage._tokens_path": ret(None),
                        "storage._client_info_path": ret(None),
                        "storage._meta_path": ret(None)})

# ── bind_issuer ───────────────────────────────────────────────────────────
case("bind_issuer", issuer="https://iss.example", state={})
case("bind_issuer", issuer="", state={})
case("bind_issuer", state={})
case("bind_issuer", issuer=0, state={})
case("bind_issuer", issuer=5, state={})
case("bind_issuer", issuer="x", state={"_bound_issuer": "old"})
case("bind_issuer", issuer=True, state={})

# ── credential_stamp ──────────────────────────────────────────────────────
CS_PATHS = {"storage._tokens_path": ret("/h/mcp-tokens/s.json"),
            "storage._client_info_path": ret("/h/mcp-tokens/s.client.json")}
ST1 = {"st_mtime_ns": 1000, "st_size": 10, "st_ino": 1}
ST2 = {"st_mtime_ns": 2000, "st_size": 20, "st_ino": 2}
case("credential_stamp", io={**CS_PATHS, "path.stat": [ret(ST1), ret(ST2)]})
case("credential_stamp", io={**CS_PATHS, "path.stat": [oserr("no file", 2), ret(ST2)]})
case("credential_stamp", io={**CS_PATHS, "path.stat": [ret(ST1), oserr("no file", 2)]})
case("credential_stamp", io={**CS_PATHS, "path.stat": [oserr("a", 2), oserr("b", 2)]})
case("credential_stamp", io={**CS_PATHS, "path.stat": raise_("KeyboardInterrupt")})
case("credential_stamp", io={**CS_PATHS, "storage._tokens_path": raise_("FileNotFoundError", "gone", errno=2)})
case("credential_stamp", io={**CS_PATHS, "path.stat": ret({"st_mtime_ns": 1000, "st_ino": 1})})
case("credential_stamp", io={**CS_PATHS, "storage._tokens_path": ret(None)})
case("credential_stamp", io={**CS_PATHS, "path.stat": [raise_("PermissionError", "denied", errno=13), ret(ST2)]})

# ── stdin_is_console ──────────────────────────────────────────────────────
case("stdin_is_console", stdin={}, platform="posix", io={"sys.stdin.isatty": ret(True)})
case("stdin_is_console", stdin={}, platform="posix", io={"sys.stdin.isatty": ret(False)})
case("stdin_is_console", stdin={}, platform="posix", io={"sys.stdin.isatty": raise_("ValueError", "closed")})
case("stdin_is_console", stdin={}, platform="posix", io={"sys.stdin.isatty": raise_("AttributeError", "no")})
case("stdin_is_console", stdin={}, platform="posix", io={"sys.stdin.isatty": raise_("KeyboardInterrupt")})
case("stdin_is_console", stdin=None, platform="posix", io={})
NT_OK = {"sys.stdin.isatty": ret(True), "sys.stdin.fileno": ret(0),
         "msvcrt.get_osfhandle": ret(123), "ctypes.c_ulong": ret(0),
         "ctypes.c_void_p": ret(0), "ctypes.byref": ret(0),
         "ctypes.windll.kernel32.GetConsoleMode": ret(1)}
case("stdin_is_console", stdin={}, platform="nt", io=dict(NT_OK))
case("stdin_is_console", stdin={}, platform="nt",
     io={**NT_OK, "ctypes.windll.kernel32.GetConsoleMode": ret(0)})
case("stdin_is_console", stdin={}, platform="nt", io={**NT_OK, "sys.stdin.fileno": oserr("bad fd", 9)})
case("stdin_is_console", stdin={}, platform="nt",
     io={**NT_OK, "ctypes.windll.kernel32.GetConsoleMode": raise_("RuntimeError", "no console")})
case("stdin_is_console", stdin={}, platform="nt", ctypes_missing=True, io={"sys.stdin.isatty": ret(True)})
case("stdin_is_console", stdin={}, platform="nt", msvcrt_missing=True, io={"sys.stdin.isatty": ret(True)})
case("stdin_is_console", stdin={}, platform="nt",
     io={**NT_OK, "ctypes.windll.kernel32.GetConsoleMode": raise_("KeyboardInterrupt")})
case("stdin_is_console", stdin={}, platform="posix", io={"sys.stdin.isatty": raise_("RuntimeError", "boom")})
case("stdin_is_console", stdin={}, platform="nt", io={**NT_OK, "ctypes.c_ulong": raise_("ValueError", "bad")})

# ── do_get ────────────────────────────────────────────────────────────────
case("do_get", path="/callback?code=c1&state=s1", io={"claim_result": ret(True)})
case("do_get", path="/callback?error=access_denied&state=s", io={"claim_result": ret(True)})
case("do_get", path="/favicon.ico", io={})
case("do_get", path="/callback?code=c", io={"claim_result": ret(False)})
case("do_get", path="/callback?code=c", io={"claim_result": raise_("RuntimeError", "boom")})
case("do_get", path="/callback?code=c", io={"claim_result": raise_("KeyboardInterrupt")})
case("do_get", path="/callback?code=c", io={"claim_result": ret(True), "send_response": oserr("write fail")})
case("do_get", path="/callback?error=%3Cscript%3E%22%27%26", io={"claim_result": ret(True)})
case("do_get", path="/callback?code=&error=e", io={"claim_result": ret(True)})
case("do_get", path="/callback?code=c&state=s&iss=https://i", io={"claim_result": ret(True)})
case("do_get", path="/callback?code=c", io={"claim_result": ret(True), "wfile.write": oserr("closed")})
case("do_get", path="http://[bad/cb?code=c", io={})

# ── log_request ───────────────────────────────────────────────────────────
case("log_request", path="/callback?code=secret", command="GET", io={"logger.debug": ret(None)})
case("log_request", path="/callback?code=secret", io={})
case("log_request", path="/cb", command="GET", io={"logger.debug": raise_("RuntimeError", "boom")})
case("log_request", path="http://[bad/cb", command="GET", io={})
case("log_request", path="/cb", command="GET", io={"logger.debug": raise_("KeyboardInterrupt")})
case("log_request", path="/cb", command=None, io={"logger.debug": ret(None)})

# ── log_message ───────────────────────────────────────────────────────────
case("log_message", fmt="%s", args=["a"], io={"logger.debug": ret(None)})
case("log_message", fmt="%s %s", args=["a", "b"], io={"logger.debug": ret(None)})
case("log_message", fmt="%%", args=[], io={"logger.debug": ret(None)})
case("log_message", fmt="%s", args=[], io={})
case("log_message", fmt="%s", args=["a", "b"], io={})
case("log_message", fmt="%d", args=["a"], io={})
case("log_message", fmt="%r", args=["a"], io={"logger.debug": ret(None)})
case("log_message", fmt="%s", args=[None], io={"logger.debug": ret(None)})
case("log_message", fmt="%s", args=["a"], io={"logger.debug": raise_("ValueError", "bad")})
case("log_message", fmt="%d", args=[3], io={"logger.debug": ret(None)})
case("log_message", fmt="no fmt", args=[], io={"logger.debug": ret(None)})
case("log_message", fmt="%s", args=[True], io={"logger.debug": ret(None)})

# ── announce_authorization_url ────────────────────────────────────────────
ANN_BASE = {"os.getenv": [ret(None), ret(None)], "can_open_browser": ret(True),
            "webbrowser.open": ret(True), "print": ret(None)}
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     redirect_uri=None, redirect_host=None, io=dict(ANN_BASE))
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     io={**ANN_BASE, "webbrowser.open": ret(False)})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     io={**ANN_BASE, "webbrowser.open": raise_("Exception", "no browser")})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     io={**ANN_BASE, "webbrowser.open": raise_("KeyboardInterrupt")})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     io={**ANN_BASE, "can_open_browser": ret(False)})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     redirect_uri="https://proxy.example/cb",
     io={**ANN_BASE, "os.getenv": [ret("1.2.3.4 5 6"), ret(None)]})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     io={**ANN_BASE, "os.getenv": [ret("1.2.3.4 5 6"), ret(None)]})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=27890,
     redirect_host="localhost",
     io={**ANN_BASE, "os.getenv": [ret(None), ret("/dev/pts/0")]})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=0,
     io={**ANN_BASE, "os.getenv": [ret("ssh"), ret(None)]})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     io={**ANN_BASE, "print": oserr("stderr closed")})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     io={**ANN_BASE, "can_open_browser": raise_("RuntimeError", "boom")})
case("announce_authorization_url", authorization_url="https://auth.example/x", port=8080,
     io={**ANN_BASE, "os.getenv": oserr("env fail")})

# ── redirect_handler ──────────────────────────────────────────────────────
RH_ANN = {"os.getenv": [ret(None), ret(None)], "can_open_browser": ret(True),
          "webbrowser.open": ret(True), "print": ret(None)}
case("redirect_handler", authorization_url="https://auth.example/x", port=8080,
     io={"get_dashboard_oauth_flow": ret("df"),
         "dashboard_flow.publish_authorization_url": ret(None)})
case("redirect_handler", authorization_url="https://auth.example/x", port=8080,
     io={"get_dashboard_oauth_flow": ret("df"),
         "dashboard_flow.publish_authorization_url": raise_("RuntimeError", "boom")})
case("redirect_handler", authorization_url="https://auth.example/x", port=8080,
     io={"get_dashboard_oauth_flow": ret(None),
         "raise_if_non_interactive": raise_("OAuthNonInteractiveError",
             "MCP OAuth requires browser authorization but no interactive session is available "
             "(non-interactive/background context). Run `hermes mcp login <server>` interactively "
             "to (re)authorize, then restart or reload the gateway.")})
case("redirect_handler", authorization_url="https://auth.example/x", port=8080,
     io={"get_dashboard_oauth_flow": ret(None), "raise_if_non_interactive": ret(None), **RH_ANN})
case("redirect_handler", authorization_url="https://auth.example/x", port=8080,
     io={"get_dashboard_oauth_flow": ret("df"),
         "dashboard_flow.publish_authorization_url": raise_("KeyboardInterrupt")})
case("redirect_handler", authorization_url="https://auth.example/x", port=8080,
     io={"get_dashboard_oauth_flow": ret(None), "raise_if_non_interactive": ret(None),
         **RH_ANN, "can_open_browser": ret(False)})
case("redirect_handler", authorization_url="https://auth.example/x", port=8080,
     io={"get_dashboard_oauth_flow": raise_("RuntimeError", "boom")})

# ── settle ────────────────────────────────────────────────────────────────
case("settle", io={"future.done": ret(False), "future.set_result": ret(None)})
case("settle", io={"future.done": ret(True)})
case("settle", io={"future.done": raise_("RuntimeError", "loop closed")})
case("settle", io={"future.done": ret(False), "future.set_result": raise_("ValueError", "bad")})
case("settle", io={"future.done": ret(False), "future.set_result": raise_("KeyboardInterrupt")})
case("settle", io={"future.done": [ret(False)], "future.set_result": [ret(None)]})

# ── close ─────────────────────────────────────────────────────────────────
CLOSE_OK = {"server.shutdown": ret(None), "server.server_close": ret(None),
            "loop.call_soon_threadsafe": ret(None), "future.done": ret(False),
            "future.set_result": ret(None)}
case("close", io=dict(CLOSE_OK))
case("close", io={**CLOSE_OK, "server.shutdown": raise_("Exception", "shutdown fail"),
                  "logger.debug": ret(None)})
case("close", io={**CLOSE_OK, "server.server_close": oserr("close fail"),
                  "logger.debug": ret(None)})
case("close", io={**CLOSE_OK, "server.shutdown": raise_("KeyboardInterrupt")})
case("close", io={**CLOSE_OK, "loop.call_soon_threadsafe": raise_("RuntimeError", "loop closed")})
case("close", io={**CLOSE_OK, "loop.call_soon_threadsafe": raise_("ValueError", "bad")})
case("close", io={**CLOSE_OK, "server.shutdown": raise_("Exception", "a"),
                  "server.server_close": raise_("Exception", "b"), "logger.debug": ret(None)})
case("close", io={**CLOSE_OK, "server.shutdown": raise_("Exception", "a"),
                  "logger.debug": raise_("RuntimeError", "log fail")})
case("close", io={**CLOSE_OK, "future.done": ret(True)})
case("close", io={**CLOSE_OK, "server.server_close": raise_("KeyboardInterrupt")})

# ── close_callback_server ─────────────────────────────────────────────────
CCS_BASE = {"asyncio.get_running_loop": ret("loop1"), "loop.create_future": ret("f1"),
            "threading.Thread": ret(None), "thread.start": ret(None),
            "server.shutdown": ret(None), "server.server_close": ret(None),
            "loop.call_soon_threadsafe": ret(None), "future.set_result": ret(None)}
case("close_callback_server", io={**CCS_BASE, "future.done": [ret(False), ret(True)]})
case("close_callback_server", io={**CCS_BASE, "future.done": [ret(False), ret(False), ret(True)],
                                  "asyncio.shield": [raise_("CancelledError", "cancel1")]})
case("close_callback_server", io={**CCS_BASE,
                                  "future.done": [ret(False), ret(False), ret(False), ret(True)],
                                  "asyncio.shield": [raise_("CancelledError", "c1"),
                                                     raise_("CancelledError", "c2")]})
case("close_callback_server", io={**CCS_BASE, "future.done": [ret(False), ret(True)],
                                  "asyncio.shield": raise_("KeyboardInterrupt")})
case("close_callback_server", io={**CCS_BASE, "future.done": [ret(False), ret(True)],
                                  "loop.call_soon_threadsafe": raise_("RuntimeError", "closed"),
                                  "asyncio.shield": ret(None)})
case("close_callback_server", io={**CCS_BASE, "asyncio.get_running_loop": raise_("RuntimeError", "no loop")})
case("close_callback_server", io={**CCS_BASE, "loop.create_future": oserr("no future")})
case("close_callback_server", io={**CCS_BASE, "thread.start": raise_("RuntimeError", "no thread")})
case("close_callback_server", io={**CCS_BASE, "future.done": [ret(False), ret(True)],
                                  "server.shutdown": raise_("Exception", "shutdown fail"),
                                  "logger.debug": ret(None)})

# ── wait ──────────────────────────────────────────────────────────────────
WAIT_CLOSE = {"asyncio.get_running_loop": ret("loop1"), "loop.create_future": ret("f1"),
              "threading.Thread": ret(None), "thread.start": ret(None),
              "server.shutdown": ret(None), "server.server_close": ret(None),
              "loop.call_soon_threadsafe": ret(None), "future.done": [ret(False), ret(True)],
              "future.set_result": ret(None)}
WAIT_FLOW = {"get_dashboard_oauth_flow": ret(None), "raise_if_non_interactive": ret(None),
             "make_callback_handler": ret({"handler": "H", "result": {}}),
             "start_callback_server": ret("srv1"), "is_interactive": ret(True),
             "print": ret(None), "callback_outcome": ret("R")}
case("wait", port=0, cimd_url=None, timeout=300.0,
     io={"get_dashboard_oauth_flow": ret("df"),
         "dashboard_flow.wait_for_callback": ret(["c1", "s1"]),
         "authorization_code_result": ret({"code": "c1", "state": "s1"})})
case("wait", port=8080, cimd_url=None, timeout=300.0,
     io={**WAIT_FLOW, "get_dashboard_oauth_flow": ret("df"), "result_taken": [ret(True)],
         **WAIT_CLOSE})
case("wait", port=8080, cimd_url=None, timeout=300.0,
     io={"get_dashboard_oauth_flow": ret(None),
         "raise_if_non_interactive": raise_("OAuthNonInteractiveError",
             "OAuth callback requires an interactive session but none is available "
             "(non-interactive/background context); skipping browser authorization without "
             "binding a callback listener. Run `hermes mcp login <server>` interactively "
             "to (re)authorize, then restart or reload the gateway.")})
case("wait", port=8080, cimd_url="https://cimd.example/m.json", timeout=300.0,
     io={**WAIT_FLOW, "result_taken": [ret(False), ret(True)], "asyncio.sleep": ret(None),
         **WAIT_CLOSE})
case("wait", port=8080, cimd_url=None, timeout=1.0,
     io={**WAIT_FLOW, "result_taken": [ret(False), ret(False)], "asyncio.sleep": ret(None),
         **WAIT_CLOSE})
case("wait", port=8080, cimd_url=None, timeout=300.0,
     io={**WAIT_FLOW, "start_callback_server": raise_("OAuthNonInteractiveError",
         "OAuth callback port 8080 is already in use")})
case("wait", port=8080, cimd_url=None, timeout=300.0,
     io={**WAIT_FLOW, "result_taken": [ret(False)],
         "asyncio.sleep": raise_("CancelledError", "cancelled"), **WAIT_CLOSE})
case("wait", port=8080, cimd_url=None, timeout=300.0,
     io={**WAIT_FLOW, "result_taken": [ret(True)],
         "callback_outcome": raise_("RuntimeError", "OAuth authorization failed: access_denied"),
         **WAIT_CLOSE})
case("wait", port=8080, cimd_url=None, timeout=300.0,
     io={**WAIT_FLOW, "is_interactive": ret(False), "result_taken": [ret(True)], **WAIT_CLOSE})
case("wait", port=0, cimd_url=None, timeout=300.0,
     io={**WAIT_FLOW, "result_taken": [ret(True)], **WAIT_CLOSE})
case("wait", port=8080, cimd_url=None, timeout=300.0,
     io={**WAIT_FLOW, "result_taken": raise_("ValueError", "bad result"), **WAIT_CLOSE})
case("wait", port=8080, cimd_url=None, timeout=300.0,
     io={**WAIT_FLOW, "make_callback_handler": ret(None)})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")
    thin = [k for k, v in counts.items() if v < 5]
    if thin:
        raise SystemExit(f"FAIL: <5 cases for {thin}")


if __name__ == "__main__":
    main()