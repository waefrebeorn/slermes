#!/usr/bin/env python3
"""Generate tests/fixtures/web_server_flow2.jsonl — scripted-seam FLOW cases for
the web_server_flow2 port (hermes_cli/web_server.py REMAINING UNCLAIMED surface:
uvicorn build / post-bind arming / server-runtime orchestrators + nested
closures + lambdas).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m[, "code": c]} raises that exception class
in the live drive and is delivered as a raise descriptor to the C universal
seam. A MISSING seam key fail-closes (RuntimeError) on BOTH sides.

Domains (documented in the port header):
- ASCII strings, canonical POSIX paths; integral ints (no integral-float literals
  in type-name-sensitive / f-string positions). float()-string bad tokens are
  printable ASCII without quotes/backslashes.
- load_config scripts dict-or-null; dashboard section dict-or-falsy. own_home /
  ssh_lock_path paths are non-empty POSIX strings.
- config/server fields are JSON objects; tracker/loop opaque markers.
- SystemExit is produced by the port-bind-conflict branch (code 75), not scripted.
Run: python tests/gen_web_server_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "web_server_flow2.jsonl")
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


def sc(v):
    """Wrap a list/tuple RESULT so the queue semantics return it whole."""
    return {"__script__": True, "ret": v}


# ── seam bundles ──

def build_seams(ssh=False, **over):
    s = {
        "load_config": {"dashboard": {}},
        "dashboard_forwarded_allow_ips": "127.0.0.1",
        "uvicorn_console_logging": None,
        "uvicorn_config": {"loaded": True},
        "uvicorn_server": {},
    }
    if ssh:
        s.update({
            "import_TUNNEL_WS_PING_INTERVAL_S": 60.0,
            "import_TUNNEL_WS_PING_TIMEOUT_S": 600.0,
            "import_IdleClientTracker": None,
            "import_wrap_asgi_with_ws_tracking": None,
            "idle_client_tracker": {"__tracker__": "t1"},
            "wrap_asgi_with_ws_tracking": {"__wrapped__": "w1"},
        })
    s.update(over)
    return s


def oss_seams(headless=False, desktop=False, ssh=False, mcp=False, **over):
    """_on_server_started base bundle."""
    # is_desktop_owned_backend: L1235 always; L1252 only when not ssh_present
    if ssh:
        dob = [False]            # second call short-circuited
    else:
        dob = [desktop, desktop]
    s = {
        "is_desktop_owned_backend": dob,
        "best_effort": None,
        "start_parent_death_watchdog": None,
        "read_bound_port": 9119,
        "write_dashboard_ready_file": None,
        "write_machine_sentinel_line": None,
        "maybe_open_browser": None,
        "get_running_loop": {"__loop__": "main"},
        "loop_time": 100,
        "loop_call_later": None,
    }
    if not headless:
        s["import_format_url_host"] = None
        s["format_url_host"] = "127.0.0.1"
    s["print"] = None
    if ssh:
        s.update({
            "import_DEFAULT_IDLE_GRACE_S": 900.0,
            "import_start_idle_watchdog": None,
            "load_config": {"dashboard": {"ssh_isolated_idle_grace_s": 300}},
            "start_idle_watchdog": None,
            "import_start_owner_watchdog": None,
            "start_owner_watchdog": None,
        })
    if ssh or desktop:
        s["import_start_code_skew_watchdog"] = None
        s["start_code_skew_watchdog"] = None
    if mcp:
        s["import_defer_background_mcp_discovery"] = None
        s["defer_background_mcp_discovery"] = None
    s.update(over)
    return s


def runtime_seams(ssh=False, **over):
    s = {
        "import_restore_user_proxy_env": None,
        "restore_user_proxy_env": None,
        "apply_ssh_session_token": None,
        "apply_ssh_owner_nonce": None,
        "import_apply_nofile_soft_limit": None,
        "apply_nofile_soft_limit": None,
        "import_start_nous_auth_keepalive": None,
        "start_nous_auth_keepalive": None,
        "configure_auth_gate": None,
        "import_record_launch_scope": None,
        "is_desktop_owned_backend": False,
        "record_launch_scope": None,
        "import_install_exit_flush_signal_handlers": None,
        "install_exit_flush_signal_handlers": None,
        "port_bind_conflict": False,
        "import_activate_multi_profile_hosting_eagerly": None,
        "activate_multi_profile_hosting_eagerly": None,
        "run_serve": None,
    }
    s.update(build_seams(ssh=ssh))
    s.update(over)
    return s


def serve_seams(headless=False, **over):
    s = {
        "config_load": None,
        "lifespan_class": {"__callable__": "lifespan"},
        "capture_signals": None,
        "capture_signals_enter": None,
        "server_startup": None,
        "server_main_loop": None,
        "server_shutdown": None,
        "capture_signals_exit": None,
    }
    s.update(oss_seams(headless=headless))
    if headless:
        s["import_record_process_ready"] = None
        s["record_process_ready"] = None
    s.update(over)
    return s


# ── _build_uvicorn_server ──
add("build_uvicorn_server", {"host": "127.0.0.1", "port": 9119, "app_state": {"auth_required": False}},
    build_seams())
add("build_uvicorn_server", {"host": "0.0.0.0", "port": 8080, "app_state": {"auth_required": False}},
    build_seams(load_config={"dashboard": {"ws_ping_interval": 30, "ws_ping_timeout": 40}}))
add("build_uvicorn_server", {"host": "0.0.0.0", "port": 8080, "app_state": {"auth_required": True}},
    build_seams(load_config={"dashboard": {"ws_ping_interval": "abc", "ws_ping_timeout": 25.5}}))
add("build_uvicorn_server", {"host": "127.0.0.1", "port": 9119, "ssh_isolated": True,
                             "app_state": {"auth_required": False}}, build_seams(ssh=True))
add("build_uvicorn_server", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    build_seams(load_config=rs("RuntimeError", "cfg boom")))
add("build_uvicorn_server", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    build_seams(load_config=None))
add("build_uvicorn_server", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    build_seams(load_config=rs("KeyboardInterrupt", "stop")))
add("build_uvicorn_server", {"host": "localhost", "port": 9119, "app_state": {"auth_required": True}},
    build_seams())
add("build_uvicorn_server", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    build_seams(uvicorn_config=rs("RuntimeError", "config boom")))
add("build_uvicorn_server", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    {"load_config": {"dashboard": {}}})   # fail-closed: dashboard_forwarded_allow_ips missing

# ── _ws_ping_setting ──
add("ws_ping_setting", {"dash_cfg": {"ws_ping_interval": 30}, "key": "ws_ping_interval"}, {})
add("ws_ping_setting", {"dash_cfg": {}, "key": "ws_ping_interval"}, {})
add("ws_ping_setting", {"dash_cfg": {"ws_ping_interval": "25.5"}, "key": "ws_ping_interval"}, {})
add("ws_ping_setting", {"dash_cfg": {"ws_ping_interval": None}, "key": "ws_ping_interval"}, {})
add("ws_ping_setting", {"dash_cfg": {"ws_ping_interval": "abc"}, "key": "ws_ping_interval"}, {})
add("ws_ping_setting", {"dash_cfg": {"ws_ping_interval": {}}, "key": "ws_ping_interval"}, {})
add("ws_ping_setting", {"dash_cfg": {"ws_ping_interval": True}, "key": "ws_ping_interval"}, {})
add("ws_ping_setting", {"dash_cfg": {"ws_ping_interval": "1e2"}, "key": "ws_ping_interval", "default": 15.0}, {})
add("ws_ping_setting", {"dash_cfg": {"ws_ping_timeout": " 30 "}, "key": "ws_ping_timeout"}, {})
add("ws_ping_setting", {"dash_cfg": {"ws_ping_interval": "1_0"}, "key": "ws_ping_interval"}, {})
add("ws_ping_setting", {"dash_cfg": {"ws_ping_interval": "1_0.5_5e1"}, "key": "ws_ping_interval"}, {})

# ── _on_server_started ──
add("on_server_started", {"host": "127.0.0.1", "port": 9119, "app_state": {"auth_required": False},
                          "server": {}}, oss_seams())
add("on_server_started", {"host": "127.0.0.1", "port": 9119, "headless": True, "app_state": {},
                          "server": {}}, oss_seams(headless=True))
add("on_server_started", {"host": "127.0.0.1", "port": 9119, "app_state": {}, "server": {}},
    oss_seams(desktop=True))
add("on_server_started", {"host": "127.0.0.1", "port": 9119,
                          "app_state": {"ssh_isolated_clients": {"t": 1}},
                          "ssh_lock_path": "/h/backend.lock.json", "ssh_owner_nonce": "n1",
                          "server": {}}, oss_seams(ssh=True))
add("on_server_started", {"host": "127.0.0.1", "port": 9119,
                          "app_state": {"ssh_isolated_clients": {"t": 1}}, "server": {}},
    oss_seams(ssh=True, load_config={"dashboard": {"ssh_isolated_idle_grace_s": "abc"}}))
add("on_server_started", {"host": "127.0.0.1", "port": 9119,
                          "app_state": {"ssh_isolated_clients": {"t": 1}}, "server": {}},
    oss_seams(ssh=True, load_config=None))
add("on_server_started", {"host": "127.0.0.1", "port": 9119, "start_mcp_discovery_after_bind": True,
                          "app_state": {}, "server": {}}, oss_seams(mcp=True))
add("on_server_started", {"host": "127.0.0.1", "port": 9119, "start_mcp_discovery_after_bind": True,
                          "app_state": {}, "server": {}},
    oss_seams(mcp=True, import_defer_background_mcp_discovery=rs("RuntimeError", "mcp boom")))
add("on_server_started", {"host": "127.0.0.1", "port": 9119, "app_state": {}, "server": {}},
    oss_seams(read_bound_port=rs("KeyboardInterrupt", "stop")))
add("on_server_started", {"host": "127.0.0.1", "port": 9119, "app_state": {}, "server": {}},
    {"best_effort": None})   # fail-closed: is_desktop_owned_backend missing
add("on_server_started", {"host": "127.0.0.1", "port": 9119, "initial_profile": "work",
                          "app_state": {}, "server": {}}, oss_seams())

# ── _reap_desktop_serves ──
add("reap_desktop_serves", {}, {"import_reap_orphaned_desktop_local_serves": None,
                                "reap_orphaned_desktop_local_serves": None})
add("reap_desktop_serves", {}, {"import_reap_orphaned_desktop_local_serves": rs("ImportError", "no mod"),
                                "reap_orphaned_desktop_local_serves": None})
add("reap_desktop_serves", {}, {"import_reap_orphaned_desktop_local_serves": None,
                                "reap_orphaned_desktop_local_serves": rs("RuntimeError", "reap boom")})
add("reap_desktop_serves", {}, {"import_reap_orphaned_desktop_local_serves": None,
                                "reap_orphaned_desktop_local_serves": rs("GeneratorExit", "gen")})
add("reap_desktop_serves", {}, {"import_reap_orphaned_desktop_local_serves": rs("KeyboardInterrupt", "stop"),
                                "reap_orphaned_desktop_local_serves": None})
add("reap_desktop_serves", {}, {})   # fail-closed

# ── _reap_mcp_helpers ──
add("reap_mcp_helpers", {}, {"import_reap_orphaned_mcp_helpers": None, "reap_orphaned_mcp_helpers": None})
add("reap_mcp_helpers", {}, {"import_reap_orphaned_mcp_helpers": rs("ImportError", "no mod"),
                             "reap_orphaned_mcp_helpers": None})
add("reap_mcp_helpers", {}, {"import_reap_orphaned_mcp_helpers": None,
                             "reap_orphaned_mcp_helpers": rs("RuntimeError", "boom")})
add("reap_mcp_helpers", {}, {"import_reap_orphaned_mcp_helpers": None,
                             "reap_orphaned_mcp_helpers": rs("GeneratorExit", "gen")})
add("reap_mcp_helpers", {}, {})   # fail-closed

# ── _register_identity ──
RI_IMP = {"import_attach_self_to_kill_on_close_job": None, "import_register_self": None,
          "register_self": None, "attach_self_to_kill_on_close_job": None}
add("register_identity", {"headless": True, "host": "127.0.0.1", "actual_port": 9119,
                          "initial_profile": "work", "isolated": True}, dict(RI_IMP))
add("register_identity", {"headless": False, "host": "0.0.0.0", "actual_port": 8080,
                          "initial_profile": "", "isolated": False}, dict(RI_IMP))
add("register_identity", {"headless": False, "host": "h", "actual_port": 1, "initial_profile": None,
                          "isolated": False}, dict(RI_IMP))
add("register_identity", {"headless": True, "host": "h", "actual_port": 1, "initial_profile": "p",
                          "isolated": False}, {**RI_IMP, "import_register_self": rs("ImportError", "no")})
add("register_identity", {"headless": True, "host": "h", "actual_port": 1, "initial_profile": "p",
                          "isolated": False}, {**RI_IMP, "register_self": rs("RuntimeError", "reg boom")})
add("register_identity", {"headless": True, "host": "h", "actual_port": 1, "initial_profile": "p",
                          "isolated": False}, {**RI_IMP, "attach_self_to_kill_on_close_job": rs("OSError", "job")})
add("register_identity", {"headless": True, "host": "h", "actual_port": 1}, {})   # fail-closed

# ── _install_noise_filter ──
NF = {"import_install_loop_noise_filter": None, "get_running_loop": {"__loop__": "main"},
      "install_loop_noise_filter": None}
add("install_noise_filter", {}, dict(NF))
add("install_noise_filter", {}, {**NF, "import_install_loop_noise_filter": rs("ImportError", "no")})
add("install_noise_filter", {}, {**NF, "get_running_loop": rs("RuntimeError", "no loop")})
add("install_noise_filter", {}, {**NF, "install_loop_noise_filter": rs("RuntimeError", "install boom")})
add("install_noise_filter", {}, {**NF, "get_running_loop": rs("GeneratorExit", "gen")})
add("install_noise_filter", {}, {})   # fail-closed

# ── _loop_heartbeat ──
add("loop_heartbeat", {"loop": {"__loop__": "main"}, "expected": 98.0},
    {"loop_time": 100, "loop_call_later": None})
add("loop_heartbeat", {"loop": {"__loop__": "main"}, "expected": 90.0},
    {"loop_time": 100, "loop_call_later": None})   # drift 10 > 5 (silent log)
add("loop_heartbeat", {"loop": {"__loop__": "main"}, "expected": 98.0},
    {"loop_time": rs("RuntimeError", "time boom"), "loop_call_later": None})
add("loop_heartbeat", {"loop": {"__loop__": "main"}, "expected": None},
    {"loop_time": 100, "loop_call_later": None})   # TypeError on sub
add("loop_heartbeat", {"loop": {"__loop__": "main"}, "expected": 98.0},
    {"loop_time": 100})   # fail-closed: loop_call_later missing
add("loop_heartbeat", {"loop": {"__loop__": "main"}, "expected": 98.0, "interval": 3.0, "threshold": 1.0},
    {"loop_time": 100, "loop_call_later": None})

# ── publish_rendezvous_step lambda ──
add("publish_rendezvous_step", {"host": "127.0.0.1", "actual_port": 9119},
    {"publish_host_rendezvous": None})
add("publish_rendezvous_step", {"host": "0.0.0.0", "actual_port": 8080},
    {"publish_host_rendezvous": rs("RuntimeError", "pub boom")})
add("publish_rendezvous_step", {"host": "h", "actual_port": 1},
    {"publish_host_rendezvous": rs("GeneratorExit", "gen")})
add("publish_rendezvous_step", {"host": "h", "actual_port": 1},
    {"publish_host_rendezvous": {"ok": True}})
add("publish_rendezvous_step", {"host": "h", "actual_port": 1}, {})   # fail-closed

# ── _start_server_runtime ──
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "app_state": {}}, runtime_seams())
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    runtime_seams(port_bind_conflict=True, report_port_in_use=None))
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "ssh_session_token": "t",
                             "ssh_owner_nonce": "n", "app_state": {}}, runtime_seams(ssh=True))
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    runtime_seams(import_start_nous_auth_keepalive=rs("RuntimeError", "ka boom")))
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    runtime_seams(start_nous_auth_keepalive=rs("KeyboardInterrupt", "stop")))
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    runtime_seams(activate_multi_profile_hosting_eagerly=rs("RuntimeError", "act boom")))
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    runtime_seams(uvicorn_config=rs("RuntimeError", "build boom")))
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    runtime_seams(configure_auth_gate=rs("RuntimeError", "gate boom")))
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "initial_profile": "work",
                             "isolated": True, "app_state": {}}, runtime_seams())
add("start_server_runtime", {"host": "127.0.0.1", "port": 9119, "app_state": {}},
    {"import_restore_user_proxy_env": None})   # fail-closed

# ── _serve ──
add("serve", {"config": {"loaded": True}, "server": {"should_exit": False, "started": True},
              "app_state": {}, "host": "127.0.0.1", "port": 9119}, serve_seams())
add("serve", {"config": {"loaded": False}, "server": {"should_exit": False, "started": True},
              "app_state": {}, "host": "127.0.0.1", "port": 9119}, serve_seams())
add("serve", {"config": {"loaded": True}, "server": {"should_exit": True, "started": False},
              "app_state": {}, "host": "127.0.0.1", "port": 9119}, serve_seams())
add("serve", {"config": {"loaded": True}, "server": {"should_exit": False, "started": True},
              "app_state": {}, "host": "127.0.0.1", "port": 9119, "headless": True},
    serve_seams(headless=True))
add("serve", {"config": {"loaded": True}, "server": {"should_exit": False, "started": True},
              "app_state": {}, "host": "127.0.0.1", "port": 9119},
    serve_seams(server_startup=rs("RuntimeError", "startup boom")))
add("serve", {"config": {"loaded": True}, "server": {"should_exit": False, "started": True},
              "app_state": {}, "host": "127.0.0.1", "port": 9119},
    serve_seams(capture_signals_exit=rs("RuntimeError", "exit boom")))
add("serve", {"config": {"loaded": True}, "server": {"should_exit": False, "started": False},
              "app_state": {}, "host": "127.0.0.1", "port": 9119}, serve_seams())
add("serve", {"config": {"loaded": True}, "server": {"should_exit": False, "started": True},
              "app_state": {}, "host": "127.0.0.1", "port": 9119},
    serve_seams(server_startup=rs("KeyboardInterrupt", "stop")))

# ── _owned ──
add("cron_owned", {"own_name": "default", "own_home": "/h/.hermes"}, {"gateway_owns_cron": True})
add("cron_owned", {"own_name": "default", "own_home": "/h/.hermes"}, {"gateway_owns_cron": False})
add("cron_owned", {"own_name": "work", "own_home": "/h/w"}, {"gateway_owns_cron": rs("RuntimeError", "probe")})
add("cron_owned", {"own_name": "work", "own_home": "/h/w"}, {"gateway_owns_cron": rs("KeyboardInterrupt", "stop")})
add("cron_owned", {"own_name": "work", "own_home": "/h/w"}, {"gateway_owns_cron": {"truthy": 1}})
add("cron_owned", {"own_name": "default", "own_home": "/h"}, {})   # fail-closed

# ── profile_gate lambda ──
add("cron_profile_gate", {"name": "default", "home": "/h"}, {"gateway_owns_cron": True})
add("cron_profile_gate", {"name": "work", "home": "/h/w"}, {"gateway_owns_cron": False})
add("cron_profile_gate", {"name": "work", "home": "/h/w"}, {"gateway_owns_cron": rs("RuntimeError", "boom")})
add("cron_profile_gate", {"name": "work", "home": "/h/w"}, {"gateway_owns_cron": {"truthy": 1}})
add("cron_profile_gate", {"name": "default", "home": ""}, {"gateway_owns_cron": False})
add("cron_profile_gate", {"name": "default", "home": "/h"}, {})   # fail-closed

# ── profile_homes lambda ──
add("cron_profile_homes", {}, {"profiles_to_serve": [[["default", "/h"], ["work", "/h/w"]]]})
add("cron_profile_homes", {}, {"profiles_to_serve": [[]]})
add("cron_profile_homes", {}, {"profiles_to_serve": rs("RuntimeError", "enum boom")})
add("cron_profile_homes", {}, {"profiles_to_serve": None})   # TypeError not iterable
add("cron_profile_homes", {}, {"profiles_to_serve": rs("GeneratorExit", "gen")})
add("cron_profile_homes", {}, {})   # fail-closed

# ── profile_homes_fallback lambda ──
add("cron_own_home_fallback", {"own_name": "default", "own_home": "/h/.hermes"}, {})
add("cron_own_home_fallback", {"own_name": "work", "own_home": "/h/w"}, {})
add("cron_own_home_fallback", {"own_name": "default", "own_home": "/h"}, {})
add("cron_own_home_fallback", {"own_name": "p2", "own_home": "/h/p2"}, {})
add("cron_own_home_fallback", {"own_name": "default", "own_home": "/h/.hermes", "extra": 1}, {})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    for c in CASES:
        if c["func"] in ("build_uvicorn_server", "start_server_runtime"):
            st = c["in"].setdefault("app_state", {})
            st.setdefault("auth_required", False)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-28s %d" % (k, counts[k]))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()