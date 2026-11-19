#!/usr/bin/env python3
"""Generate tests/fixtures/web_server_flow.jsonl — scripted-seam FLOW cases for
the web_server_flow port (hermes_cli/web_server.py UNCLAIMED web-server core:
app-state leaves, ssh bootstrap, cron ownership, token guard, middleware chain,
health leaf, CORS origin, startup auth gate, best-effort / rendezvous, serve
runner).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m[, "code": c]} raises that exception class
in the live drive and is delivered as a raise descriptor to the C universal
seam. A MISSING seam key fail-closes (RuntimeError) on BOTH sides.

Domains (documented in the port header):
- ASCII strings, canonical POSIX paths; integral ints (no integral-float
  literals in type-name-sensitive / f-string positions).
- request schema {state, app:{state}, headers, query_params, url:{path}, scope}.
- `is True` identity gates are JSON bool true only.
- Truthiness-gated records are null or NON-EMPTY dicts (owner={} only exercises
  the reclaim .pid AttributeError branch; publish read_record uses null/non-empty).
- claim_host_lock scripts are 2-element [outcome_token, error] arrays; tokens
  are "acquired"/"held-by-other"/"could-not-open". stat scripts carry st_dev/st_ino.
- SystemExit raise specs carry an int/str "code"; call_next scripts for the
  health middleware carry an int status_code (or __raise__).
Run: python tests/gen_web_server_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "web_server_flow.jsonl")

CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg="", code=None):
    d = {"__raise__": cls, "msg": msg}
    if code is not None:
        d["code"] = code
    return d


def req(state=None, app_state=None, headers=None, path="", scope=None, query=None):
    return {"state": state or {}, "app": {"state": app_state or {}},
            "headers": headers or {}, "query_params": query or {},
            "url": {"path": path}, "scope": scope or {}}


KEYS = ["basic", "dashboard_auth/basic"]
NOAUTH_BASE = {"import_nous": None, "nous_last_skip_reason": None,
               "import_load_config": None, "import_BASIC_AUTH_PLUGIN_KEYS": KEYS,
               "load_config": {}}


def noauth_seams(host_loopback=True, **over):
    s = dict(NOAUTH_BASE)
    if host_loopback:
        s["import_resolve_public_url"] = None
        s["resolve_public_url"] = None
    s.update(over)
    return s


# ── _gateway_owns_cron ──
G_IMP = {"import_check_gateway_running": None, "import_served_by_running_multiplexer": None}
add("gateway_owns_cron", {"name": "default", "home": "/h/.hermes"},
    {**G_IMP, "check_gateway_running": True})
add("gateway_owns_cron", {"name": "default", "home": "/h"},
    {**G_IMP, "check_gateway_running": False})
add("gateway_owns_cron", {"name": "work", "home": "/h"},
    {**G_IMP, "check_gateway_running": False, "served_by_running_multiplexer": True})
add("gateway_owns_cron", {"name": "work", "home": "/h"},
    {**G_IMP, "check_gateway_running": False, "served_by_running_multiplexer": False})
add("gateway_owns_cron", {"name": "work", "home": "/h"},
    {**G_IMP, "check_gateway_running": True})
add("gateway_owns_cron", {"name": "default", "home": "/h"},
    {**G_IMP, "check_gateway_running": False, "served_by_running_multiplexer": True})
add("gateway_owns_cron", {"name": "work", "home": "/h"},
    {**G_IMP, "check_gateway_running": rs("RuntimeError", "probe boom")})
add("gateway_owns_cron", {"name": "work", "home": "/h"},
    {"import_check_gateway_running": rs("ImportError", "no profiles"),
     "import_served_by_running_multiplexer": None})

# ── _app_state_default ──
add("app_state_default", {"state": {"x": 1}, "name": "x", "factory": {"__callable__": "factory"}}, {})
add("app_state_default", {"state": {}, "name": "x", "factory": {"__callable__": "factory"}},
    {"app_state_factory": 42})
add("app_state_default", {"state": {"x": None}, "name": "x", "factory": {"__callable__": "factory"}}, {})
add("app_state_default", {"state": {}, "name": "x", "factory": {"__callable__": "factory"}},
    {"app_state_factory": rs("ValueError", "factory boom")})
add("app_state_default", {"state": {}, "name": "x", "factory": {"__callable__": "dict"}}, {})
add("app_state_default", {"state": {"a": 1, "b": 2}, "name": "c", "factory": {"__callable__": "factory"}},
    {"app_state_factory": "v"})
add("app_state_default", {"state": {}, "name": "x", "factory": {"__callable__": "factory"}},
    {"app_state_factory": None})

# ── _get_pty_active_session_files ──
add("get_pty_active_session_files", {"state": {}}, {})
add("get_pty_active_session_files", {"state": {"pty_active_session_files": {"a": 1}}}, {})
add("get_pty_active_session_files", {"state": {"pty_active_session_files": None}}, {})
add("get_pty_active_session_files", {"state": {"other": 1}}, {})
add("get_pty_active_session_files", {"state": {"pty_active_session_files": {}}}, {})
add("get_pty_active_session_files", {"state": {"pty_active_session_files": [1, 2]}}, {})

# ── _apply_ssh_session_token ──
add("apply_ssh_session_token", {"state": {"_SESSION_TOKEN": "old"}, "token": "new"}, {})
add("apply_ssh_session_token", {"state": {"_SESSION_TOKEN": "old"}, "token": ""}, {})
add("apply_ssh_session_token", {"state": {"_SESSION_TOKEN": "old"}, "token": None}, {})
add("apply_ssh_session_token", {"state": {}, "token": "t"}, {})
add("apply_ssh_session_token", {"state": {"_SESSION_TOKEN": "old"}, "token": "old"}, {})
add("apply_ssh_session_token", {"state": {"_SESSION_TOKEN": ""}, "token": "x"}, {})

# ── _apply_ssh_owner_nonce ──
def nonce_full(**over):
    s = {"sysconfig_get_paths": {"purelib": "/pl"},
         "os_path_join": "/pl/.hermes-ssh-runtime-n",
         "open_marker": None, "marker_file_enter": None, "os_getpid": 1234,
         "marker_file_write": None, "marker_file_exit": None,
         "os_stat": {"st_dev": 1, "st_ino": 2}}
    s.update(over)
    return s


add("apply_ssh_owner_nonce", {"state": {}, "nonce": None}, {})
add("apply_ssh_owner_nonce", {"state": {}, "nonce": ""}, {})
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"}, nonce_full())
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"},
    {"sysconfig_get_paths": rs("KeyError", "'purelib'")})
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"}, {"sysconfig_get_paths": {}})
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"},
    nonce_full(open_marker=rs("OSError", "read-only")))
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"},
    nonce_full(marker_file_write=rs("OSError", "write fail")))
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"},
    nonce_full(marker_file_exit=rs("OSError", "close fail")))
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"},
    nonce_full(os_stat=rs("OSError", "stat fail")))
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"},
    {"sysconfig_get_paths": {"purelib": "/pl"},
     "os_path_join": "/pl/.hermes-ssh-runtime-n",
     "open_marker": rs("RuntimeError", "boom"), "marker_file_exit": None})
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"},
    {"sysconfig_get_paths": rs("OSError", "paths fail")})
add("apply_ssh_owner_nonce", {"state": {}, "nonce": "n"},
    {"sysconfig_get_paths": {"purelib": "/pl"},
     "os_path_join": "/pl/.hermes-ssh-runtime-n", "open_marker": None,
     "marker_file_enter": None, "os_getpid": rs("KeyboardInterrupt"),
     "marker_file_exit": None})

# ── _require_token ──
add("require_token", {"request": req(state={"native_http_principal": "p"})}, {})
add("require_token", {"request": req(state={"hosted_http_authenticated": True})}, {})
add("require_token", {"request": req(state={"hosted_http_authenticated": 1})},
    {"has_valid_session_token": True})
add("require_token", {"request": req(state={"session": {"u": 1}}, app_state={"auth_required": True})}, {})
add("require_token", {"request": req(app_state={"auth_required": True})}, {})
add("require_token", {"request": req(state={"session": None}, app_state={"auth_required": True})}, {})
add("require_token", {"request": req()}, {"has_valid_session_token": False})
add("require_token", {"request": req()}, {"has_valid_session_token": True})
add("require_token", {"request": req(state={"native_http_principal": None, "hosted_http_authenticated": None})},
    {"has_valid_session_token": True})
add("require_token", {"request": req()}, {"has_valid_session_token": rs("RuntimeError", "boom")})

# ── host_header_middleware ──
add("host_header_middleware", {"request": req(), "app_state": {}}, {"call_next": {"status_code": 200}})
add("host_header_middleware", {"request": req(headers={"host": "127.0.0.1"}), "app_state": {"bound_host": "127.0.0.1"}},
    {"is_accepted_host": True, "call_next": {"status_code": 200}})
add("host_header_middleware", {"request": req(headers={"host": "evil.com"}), "app_state": {"bound_host": "127.0.0.1"}},
    {"is_accepted_host": False})
add("host_header_middleware", {"request": req(headers={"host": "a.com"}),
                               "app_state": {"bound_host": "127.0.0.1", "trusted_public_hosts": ["a.com"]}},
    {"is_accepted_host": True, "call_next": {"status_code": 200}})
add("host_header_middleware", {"request": req(headers={"host": "x"}), "app_state": {"bound_host": "127.0.0.1"}},
    {"is_accepted_host": True, "call_next": {"status_code": 200}})
add("host_header_middleware", {"request": req(), "app_state": {"bound_host": ""}},
    {"call_next": {"status_code": 200}})
add("host_header_middleware", {"request": req(), "app_state": {"bound_host": "127.0.0.1"}},
    {"is_accepted_host": rs("RuntimeError", "boom")})
add("host_header_middleware", {"request": req(), "app_state": {"bound_host": "127.0.0.1", "trusted_public_hosts": None}},
    {"is_accepted_host": True, "call_next": {"status_code": 200}})

# ── _plugin_api_runtime_gate ──
def pgate_seams(**over):
    s = {"plugin_api_name": "p", "plugin_request_scope": None, "plugin_scope_enter": None,
         "plugin_api_request_refused": False, "forward_secondary_plugin_request": None,
         "plugin_scope_exit": None, "call_next": {"status_code": 200}}
    s.update(over)
    return s


add("plugin_api_runtime_gate", {"request": req()},
    {"plugin_api_name": None, "call_next": {"status_code": 200}})
add("plugin_api_runtime_gate", {"request": req(state={"token_authenticated": True})}, pgate_seams())
add("plugin_api_runtime_gate", {"request": req(state={"token_authenticated": True})},
    pgate_seams(plugin_api_request_refused=True))
add("plugin_api_runtime_gate", {"request": req()},
    pgate_seams(has_valid_session_token=True, forward_secondary_plugin_request={"status_code": 201}))
add("plugin_api_runtime_gate", {"request": req()},
    pgate_seams(has_valid_session_token=False, has_valid_query_token=True))
add("plugin_api_runtime_gate", {"request": req()},
    {"plugin_api_name": "p", "has_valid_session_token": False, "has_valid_query_token": False,
     "call_next": {"status_code": 200}})
add("plugin_api_runtime_gate", {"request": req(app_state={"auth_required": True})}, pgate_seams())
add("plugin_api_runtime_gate", {"request": req(state={"token_authenticated": True})},
    pgate_seams(forward_secondary_plugin_request=rs("RuntimeError", "boom")))
add("plugin_api_runtime_gate", {"request": req(state={"token_authenticated": True})},
    pgate_seams(plugin_scope_exit=rs("OSError", "scope fail")))
add("plugin_api_runtime_gate", {"request": req(state={"token_authenticated": True})},
    pgate_seams(forward_secondary_plugin_request={}))

# ── _dashboard_auth_gate ──
add("dashboard_auth_gate", {"request": req()},
    {"import_gated_auth_middleware": None, "gated_auth_middleware": {"status_code": 200}})
add("dashboard_auth_gate", {"request": req()},
    {"import_gated_auth_middleware": None, "gated_auth_middleware": None})
add("dashboard_auth_gate", {"request": req()},
    {"import_gated_auth_middleware": rs("ImportError", "no middleware")})
add("dashboard_auth_gate", {"request": req()},
    {"import_gated_auth_middleware": None, "gated_auth_middleware": rs("RuntimeError", "boom")})
add("dashboard_auth_gate", {"request": req()},
    {"import_gated_auth_middleware": None, "gated_auth_middleware": {"status_code": 401}})
add("dashboard_auth_gate", {"request": req(path="/api/x")},
    {"import_gated_auth_middleware": None, "gated_auth_middleware": {"status_code": 204}})

# ── auth_middleware ──
add("auth_middleware", {"request": req(path="/api/health")}, {"call_next": {"status_code": 200}})
add("auth_middleware", {"request": req(path="/api/sessions")},
    {"has_valid_session_token": True, "call_next": {"status_code": 200}})
add("auth_middleware", {"request": req(path="/api/sessions")},
    {"has_valid_session_token": False, "has_valid_query_token": False})
add("auth_middleware", {"request": req(state={"token_authenticated": True}, path="/api/sessions")},
    {"call_next": {"status_code": 200}})
add("auth_middleware", {"request": req(app_state={"auth_required": True}, path="/api/sessions")},
    {"call_next": {"status_code": 200}})
add("auth_middleware", {"request": req(path="/notapi")}, {"call_next": {"status_code": 200}})
add("auth_middleware", {"request": req(path="/api/mcp/oauth/callback/x")},
    {"call_next": {"status_code": 200}})
add("auth_middleware", {"request": req(path="/api/sessions")},
    {"has_valid_session_token": False, "has_valid_query_token": True, "call_next": {"status_code": 200}})
add("auth_middleware", {"request": req(path="/api/status")}, {"call_next": {"status_code": 200}})
add("auth_middleware", {"request": req(state={"native_http_principal": "p"}, path="/api/sessions")},
    {"call_next": {"status_code": 200}})
add("auth_middleware", {"request": req(path="/api/sessions")},
    {"has_valid_session_token": rs("RuntimeError", "boom")})

# ── _token_auth_seam ──
T_IMP = {"import_is_peer_path": None, "import_peer_pass_middleware": None}
T_IMP2 = {"import_authenticate_native_http": None, "import_native_profile_scope": None,
          "import_token_auth_middleware": None}
add("token_auth_seam", {"request": req(path="/api/peer/x")},
    {**T_IMP, "is_peer_path": True, "peer_pass_middleware": {"status_code": 200}})
add("token_auth_seam", {"request": req()},
    {**T_IMP, **T_IMP2, "is_peer_path": False, "authenticate_native_http": {"status_code": 401}})
add("token_auth_seam", {"request": req()},
    {**T_IMP, **T_IMP2, "is_peer_path": False, "authenticate_native_http": None,
     "native_profile_scope": None, "native_scope_enter": None,
     "token_auth_middleware": {"status_code": 200}, "native_scope_exit": None})
add("token_auth_seam", {"request": req()},
    {**T_IMP, **T_IMP2, "is_peer_path": False, "authenticate_native_http": None,
     "native_profile_scope": None, "native_scope_enter": None,
     "token_auth_middleware": rs("RuntimeError", "boom"), "native_scope_exit": None})
add("token_auth_seam", {"request": req()},
    {**T_IMP, **T_IMP2, "is_peer_path": False, "authenticate_native_http": None,
     "native_profile_scope": None, "native_scope_enter": None,
     "token_auth_middleware": {"status_code": 200}, "native_scope_exit": rs("OSError", "scope fail")})
add("token_auth_seam", {"request": req()}, {"import_is_peer_path": rs("ImportError", "no peer")})
add("token_auth_seam", {"request": req()},
    {**T_IMP, **T_IMP2, "is_peer_path": False, "authenticate_native_http": rs("RuntimeError", "boom")})
add("token_auth_seam", {"request": req(path="/api/peer/x")},
    {**T_IMP, "is_peer_path": True, "peer_pass_middleware": rs("RuntimeError", "peer boom")})

# ── _dashboard_health_middleware ──
add("dashboard_health_middleware", {"request": req(path="/api/x")},
    {"call_next": {"status_code": 200}, "import_is_peer_path": None})
add("dashboard_health_middleware", {"request": req(path="/api/x")},
    {"call_next": {"status_code": 500}, "import_is_peer_path": None, "is_peer_path": False,
     "record_error": None})
add("dashboard_health_middleware", {"request": req(path="/api/peer/x")},
    {"call_next": {"status_code": 503}, "import_is_peer_path": None, "is_peer_path": True})
add("dashboard_health_middleware", {"request": req(path="/api/x")},
    {"call_next": {"status_code": 499}, "import_is_peer_path": None})
add("dashboard_health_middleware", {"request": req(path="/api/x")},
    {"call_next": rs("ValueError", "handler boom"), "import_error_type_name": None,
     "error_type_name": "ValueError", "record_error": None})
add("dashboard_health_middleware", {"request": req(path="/api/x")},
    {"call_next": rs("KeyboardInterrupt")})
add("dashboard_health_middleware", {"request": req(path="/api/x")},
    {"call_next": rs("SystemExit", "1", code=1)})
add("dashboard_health_middleware", {"request": req(path="/api/x")},
    {"call_next": {"status_code": 500}, "import_is_peer_path": None, "is_peer_path": False,
     "record_error": rs("RuntimeError", "record boom")})
add("dashboard_health_middleware", {"request": req(path="/api/x")},
    {"call_next": {}, "import_is_peer_path": None})
add("dashboard_health_middleware", {"request": req(path="/api/x")},
    {"call_next": {"status_code": 502}, "import_is_peer_path": None, "is_peer_path": False,
     "record_error": None})

# ── DashboardHealth.record_selftest ──
add("record_selftest", {"passed": True, "http_status": 200}, {"time_time": 1000})
add("record_selftest", {"passed": False, "http_status": 500}, {"time_time": 2000})
add("record_selftest", {"passed": True, "http_status": None}, {"time_time": 3000})
add("record_selftest", {"passed": False, "http_status": None}, {"time_time": 0})
add("record_selftest", {"health": {"selftest_status": "ok", "selftest_at": 5},
                        "passed": False, "http_status": 200}, {"time_time": 10})
add("record_selftest", {"passed": 1, "http_status": 200}, {"time_time": 1})
add("record_selftest", {"passed": True, "http_status": 200}, {"time_time": rs("RuntimeError", "clock boom")})

# ── _is_trusted_local_origin ──
add("is_trusted_local_origin", {"origin": "http://127.0.0.1:9119", "app_state": {}},
    {"import_is_trusted_local_origin": None, "is_trusted_local_origin": True})
add("is_trusted_local_origin", {"origin": "http://evil.com", "app_state": {}},
    {"import_is_trusted_local_origin": None, "is_trusted_local_origin": False})
add("is_trusted_local_origin", {"origin": "x", "app_state": {}},
    {"import_is_trusted_local_origin": rs("ImportError", "no boundary")})
add("is_trusted_local_origin", {"origin": "x", "app_state": {}},
    {"import_is_trusted_local_origin": None, "is_trusted_local_origin": rs("RuntimeError", "boom")})
add("is_trusted_local_origin", {"origin": "", "app_state": {}},
    {"import_is_trusted_local_origin": None, "is_trusted_local_origin": False})
add("is_trusted_local_origin", {"origin": "http://localhost:9119", "app_state": {"bound_host": "x"}},
    {"import_is_trusted_local_origin": None, "is_trusted_local_origin": True})

# ── _LocalOriginCORSMiddleware.is_allowed_origin ──
add("is_allowed_origin", {"origin": "http://127.0.0.1:9119", "app_state": {}},
    {"import_is_trusted_local_origin": None, "is_trusted_local_origin": True})
add("is_allowed_origin", {"origin": "http://evil.com", "app_state": {}},
    {"import_is_trusted_local_origin": None, "is_trusted_local_origin": False})
add("is_allowed_origin", {"origin": "x", "app_state": {}},
    {"import_is_trusted_local_origin": rs("ImportError", "no boundary")})
add("is_allowed_origin", {"origin": "x", "app_state": {}},
    {"import_is_trusted_local_origin": None, "is_trusted_local_origin": None})
add("is_allowed_origin", {"origin": "", "app_state": {}},
    {"import_is_trusted_local_origin": None, "is_trusted_local_origin": True})

# ── _no_auth_provider_message ──
add("no_auth_provider_message", {"host": "0.0.0.0", "app_state": {}}, noauth_seams(host_loopback=False))
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {}},
    noauth_seams(resolve_public_url="https://x.example"))
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {"listener_public_url": "https://own.example"}},
    noauth_seams())
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {}},
    noauth_seams(nous_last_skip_reason=["no config", "no config"]))
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {}},
    noauth_seams(load_config={"dashboard": {"basic_auth": {"username": "u", "password_hash": "h"}},
                              "plugins": {"disabled": ["basic"]}}))
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {}},
    noauth_seams(import_nous=rs("ImportError", "no nous")))
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {}},
    noauth_seams(resolve_public_url=rs("RuntimeError", "resolve boom")))
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {}},
    noauth_seams(load_config=rs("ValueError", "cfg boom")))
add("no_auth_provider_message", {"host": "::1", "app_state": {}}, noauth_seams())
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {}},
    noauth_seams(load_config={"dashboard": {"basic_auth": {"username": "u", "password": "p"}},
                              "plugins": {"disabled": ["dashboard_auth/basic"]}}))
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {}},
    noauth_seams(nous_last_skip_reason=["a", rs("RuntimeError", "read2 boom")]))
add("no_auth_provider_message", {"host": "127.0.0.1", "app_state": {"listener_public_url": "https://own.example"}},
    noauth_seams(load_config={"dashboard": {"basic_auth": {"username": "u", "password_hash": "h"}},
                              "plugins": {"disabled": ["basic"]}}))

# ── _configure_auth_gate ──
add("configure_auth_gate", {"state": {}, "host": "127.0.0.1"},
    {"dashboard_public_hosts": [[]], "dashboard_public_origins": [[]],
     "desktop_loopback_auth_exempt": True, "should_require_auth": False})
add("configure_auth_gate", {"state": {}, "host": "0.0.0.0"},
    {"dashboard_public_hosts": [[]], "dashboard_public_origins": [[]],
     "desktop_loopback_auth_exempt": False, "should_require_dashboard_auth": True,
     "import_list_providers": None, "list_providers": [[{"name": "basic"}], [{"name": "basic"}]]})
add("configure_auth_gate", {"state": {}, "host": "0.0.0.0"},
    {"dashboard_public_hosts": [[]], "dashboard_public_origins": [[]],
     "desktop_loopback_auth_exempt": False, "should_require_dashboard_auth": True,
     "import_list_providers": None, "list_providers": [[]],
     **noauth_seams(host_loopback=False)})
add("configure_auth_gate", {"state": {"listener_public_url": "https://own.example"}, "host": "127.0.0.1"},
    {"public_url_hosts": [["own.example"]], "public_url_origins": [["https://own.example"]],
     "should_require_dashboard_auth": False})
add("configure_auth_gate", {"state": {}, "host": "127.0.0.1"},
    {"dashboard_public_hosts": [[]], "dashboard_public_origins": [[]],
     "desktop_loopback_auth_exempt": True, "should_require_auth": True,
     "import_list_providers": None, "list_providers": [[{"name": "x"}], [{"name": "x"}]]})
add("configure_auth_gate", {"state": {}, "host": "127.0.0.1"},
    {"dashboard_public_hosts": rs("RuntimeError", "hosts boom")})
add("configure_auth_gate", {"state": {}, "host": "0.0.0.0"},
    {"dashboard_public_hosts": [[]], "dashboard_public_origins": [[]],
     "desktop_loopback_auth_exempt": False, "should_require_dashboard_auth": True,
     "import_list_providers": None, "list_providers": [[{"name": "a"}], [{}]]})
add("configure_auth_gate", {"state": {}, "host": "0.0.0.0", "allow_public": True},
    {"dashboard_public_hosts": [[]], "dashboard_public_origins": [[]],
     "desktop_loopback_auth_exempt": False, "should_require_dashboard_auth": True,
     "import_list_providers": None, "list_providers": [[{"name": "basic"}], [{"name": "basic"}]]})

# ── _best_effort ──
add("best_effort", {"what": "step"}, {"best_effort_fn": None})
add("best_effort", {"what": "step"}, {"best_effort_fn": 42})
add("best_effort", {"what": "step"}, {"best_effort_fn": rs("ValueError", "boom")})
add("best_effort", {"what": "step"}, {"best_effort_fn": rs("KeyboardInterrupt")})
add("best_effort", {"what": "step"}, {"best_effort_fn": rs("SystemExit", "1", code=1)})
add("best_effort", {"what": "step"}, {"best_effort_fn": rs("RuntimeError", "boom")})
add("best_effort", {"what": "step"}, {"best_effort_fn": "x"})

# ── _reclaim_host_from_orphaned_owner ──
R_IMP = {"import_host_rendezvous": None, "import_reap_orphaned_backend_owner": None}
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {**R_IMP, "read_record": None, "discard_dead_record": True})
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {**R_IMP, "read_record": None, "discard_dead_record": False})
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {**R_IMP, "read_record": None, "discard_dead_record": rs("ValueError", "boom")})
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {**R_IMP, "read_record": rs("RuntimeError", "read boom")})
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {**R_IMP, "read_record": {"pid": 1234, "create_time": 100}, "os_getpid": 1234})
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {**R_IMP, "read_record": {"pid": 999, "create_time": 100}, "os_getpid": 1234,
     "reap_orphaned_backend_owner": None})
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {**R_IMP, "read_record": {"pid": 999, "create_time": 100}, "os_getpid": 1234,
     "reap_orphaned_backend_owner": {"ok": 1}, "discard_dead_record": None})
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {**R_IMP, "read_record": {"pid": 999, "create_time": 100}, "os_getpid": 1234,
     "reap_orphaned_backend_owner": rs("ValueError", "reap boom")})
add("reclaim_host_from_orphaned_owner", {"role": "serve"}, {**R_IMP, "read_record": {}})
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {**R_IMP, "read_record": {"pid": 999, "create_time": 100}, "os_getpid": 1234,
     "reap_orphaned_backend_owner": {"ok": 1}, "discard_dead_record": rs("OSError", "disc boom")})
add("reclaim_host_from_orphaned_owner", {"role": "serve"},
    {"import_host_rendezvous": rs("ImportError", "no hr"), "import_reap_orphaned_backend_owner": None})

# ── _publish_host_rendezvous ──
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119, "session_token": "tok"},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": False,
     "claim_host_lock": [["acquired", None]], "served_profiles": [["default"]],
     "publish_record": None, "cleanup_on_exit": None})
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119, "session_token": "tok"},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": True,
     "claim_host_lock": [["acquired", None]], "served_profiles": [["default"]],
     "publish_record": None, "cleanup_on_exit": None})
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": False,
     "claim_host_lock": [["could-not-open", "err"]]})
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119, "session_token": "tok"},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": False,
     "claim_host_lock": [["held-by-other", None], ["acquired", None]],
     "import_reap_orphaned_backend_owner": None, "read_record": None,
     "discard_dead_record": True, "served_profiles": [["default"]],
     "publish_record": None, "cleanup_on_exit": None})
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": False,
     "claim_host_lock": [["held-by-other", None]],
     "import_reap_orphaned_backend_owner": None,
     "read_record": [{"pid": 1234, "create_time": 1}, {"pid": 555, "create_time": 2}],
     "os_getpid": 1234, "describe": None})
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": True,
     "claim_host_lock": [["held-by-other", None]], "read_record": {"pid": 1, "create_time": 1},
     "describe": None})
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": False,
     "claim_host_lock": [["held-by-other", None], ["held-by-other", None]],
     "import_reap_orphaned_backend_owner": None, "read_record": [None, {"pid": 7, "create_time": 3}],
     "discard_dead_record": True, "describe": None})
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": False,
     "claim_host_lock": [["acquired", None]], "served_profiles": rs("RuntimeError", "profiles boom")})
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": False,
     "claim_host_lock": rs("RuntimeError", "claim boom")})
add("publish_host_rendezvous", {"state": {}, "host": "127.0.0.1", "port": 9119},
    {"import_host_rendezvous": None, "is_desktop_owned_backend": False,
     "claim_host_lock": [["acquired", None]], "served_profiles": [["default"]],
     "publish_record": rs("RuntimeError", "publish boom")})

# ── _windows_serve_loop_factory ──
add("windows_serve_loop_factory", {}, {"get_loop_factory": None})
add("windows_serve_loop_factory", {}, {"get_loop_factory": {"__callable__": "proactor"}})
add("windows_serve_loop_factory", {}, {"get_loop_factory": {"__callable__": "selector"}})
add("windows_serve_loop_factory", {}, {"get_loop_factory": {"__callable__": "uvloop"}})
add("windows_serve_loop_factory", {}, {"get_loop_factory": {"__callable__": "custom"}})
add("windows_serve_loop_factory", {}, {"get_loop_factory": rs("RuntimeError", "boom")})

# ── _run_serve ──
add("run_serve", {"platform": "linux"}, {"serve_call": None, "asyncio_run": None})
add("run_serve", {"platform": "linux"}, {"serve_call": None, "asyncio_run": rs("KeyboardInterrupt")})
add("run_serve", {"platform": "linux", "host": "127.0.0.1", "port": 9119},
    {"serve_call": None, "asyncio_run": rs("SystemExit", "1", code=1),
     "port_bind_conflict": True, "report_port_in_use": None})
add("run_serve", {"platform": "linux", "host": "127.0.0.1", "port": 9119},
    {"serve_call": None, "asyncio_run": rs("SystemExit", "1", code=1), "port_bind_conflict": False})
add("run_serve", {"platform": "linux"},
    {"serve_call": None, "asyncio_run": rs("SystemExit", "2", code=2)})
add("run_serve", {"platform": "linux"}, {"serve_call": None, "asyncio_run": rs("ValueError", "boom")})
add("run_serve", {"platform": "win32"},
    {"import_asyncio_run": None, "get_loop_factory": None, "serve_call": None,
     "uvicorn_asyncio_run": None})
add("run_serve", {"platform": "win32"},
    {"import_asyncio_run": rs("ImportError", "no compat"), "windows_selector_policy": None,
     "set_event_loop_policy": None, "serve_call": None, "asyncio_run": None})
add("run_serve", {"platform": "win32"},
    {"import_asyncio_run": rs("ImportError", "no compat"),
     "windows_selector_policy": rs("RuntimeError", "policy boom"),
     "serve_call": None, "asyncio_run": None})
add("run_serve", {"platform": "win32", "host": "127.0.0.1", "port": 9119},
    {"import_asyncio_run": None, "get_loop_factory": {"__callable__": "proactor"},
     "serve_call": None, "uvicorn_asyncio_run": rs("SystemExit", "1", code=1),
     "port_bind_conflict": True, "report_port_in_use": None})
add("run_serve", {"platform": "linux"}, {"serve_call": rs("KeyboardInterrupt")})
add("run_serve", {"platform": "linux", "host": "127.0.0.1", "port": 9119},
    {"serve_call": rs("SystemExit", "1", code=1), "port_bind_conflict": True,
     "report_port_in_use": None})

# ── start_server ──
S_IMP = {"import_suppress_interactive_install_prompts": None, "suppress_prompts_cm": None,
         "suppress_prompts_enter": None, "suppress_prompts_exit": None}
add("start_server", {"args": [], "kwargs": {}}, {**S_IMP, "start_server_runtime": None})
add("start_server", {"args": ["127.0.0.1"], "kwargs": {"port": 9119}},
    {**S_IMP, "start_server_runtime": {"ok": 1}})
add("start_server", {"args": [], "kwargs": {}},
    {**S_IMP, "start_server_runtime": rs("ValueError", "boom")})
add("start_server", {"args": [], "kwargs": {}},
    {**S_IMP, "start_server_runtime": None, "suppress_prompts_exit": rs("OSError", "exit boom")})
add("start_server", {"args": [], "kwargs": {}},
    {"import_suppress_interactive_install_prompts": rs("ImportError", "no pm")})
add("start_server", {"args": [], "kwargs": {}},
    {**S_IMP, "start_server_runtime": rs("KeyboardInterrupt")})
add("start_server", {"args": [1, 2], "kwargs": {"a": "b"}},
    {**S_IMP, "start_server_runtime": "done"})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if thin:
        print("WARNING <5 cases: %s" % thin)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
