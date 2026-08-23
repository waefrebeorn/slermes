#!/usr/bin/env python3
"""
sta_oracle_final10.py — LIVE-Python twin for t_port_final10.c.

Recomputes each fixture case from the real upstream modules:
  hermes_cli/relay_plugin_cutover.py, hermes_cli/worktree_cmd.py,
  hermes_cli/process_identity.py, gateway/browser_control_artifacts.py,
  gateway/browser_control_broker.py, gateway/control_socket.py,
  hermes_cli/update_inventory.py, tools/browser_extension_router.py
and emits one JSON line per case, byte-equal to the C harness's output.
"""
import hashlib
import importlib.util
import json
import os
import sys

_repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if _repo not in sys.path:
    sys.path.insert(0, _repo)


def _load(rel, name):
    cand = os.path.join(_repo, rel)
    spec = importlib.util.spec_from_file_location(name, cand)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


cutover = _load("hermes_cli/relay_plugin_cutover.py", "live_cutover")
wtcmd = _load("hermes_cli/worktree_cmd.py", "live_wtcmd")
procid = _load("hermes_cli/process_identity.py", "live_procid")
artifacts = _load("gateway/browser_control_artifacts.py", "live_artifacts")
broker = _load("gateway/browser_control_broker.py", "live_broker")
control_socket = _load("gateway/control_socket.py", "live_ctlsock")
inventory = _load("hermes_cli/update_inventory.py", "live_inventory")
router = _load("tools/browser_extension_router.py", "live_router")


class FakeBroker:
    def __init__(self, scope_return, lane_registered, select_return,
                 dispatch_result):
        self._scope = object() if scope_return else None
        self._lane = lane_registered
        self._select = select_return
        self._result = dispatch_result

    def scope_for_session(self, **identity):
        return self._scope

    def select(self, scope, capability):
        return "ctrl" if (scope is not None and self._select) else None

    def dispatch(self, scope, *, action, arguments, tool_call_id):
        return self._result

    def lane_registered(self, **identity):
        return self._lane


class ControllerUnavailable(Exception):
    pass




def out(op, payload):
    print(json.dumps({"op": op, "out": payload}, ensure_ascii=False))


with open(sys.argv[1], encoding="utf-8") as fh:
    cases = json.load(fh)
if isinstance(cases, dict):
    cases = [cases]
for c in cases:
    op = c["op"]
    if op == "cutover_keys":
        vals = [v for v in c["values"] if isinstance(v, str)]
        # C harness passes only strings; non-strings are dropped there too.
        keys = list(cutover.legacy_relay_plugin_keys(list(c["values"])))
        out(op, keys)
    elif op == "cutover_env":
        env = {p["name"]: p.get("value") for p in c.get("env", [])} or None
        out(op, list(cutover.configured_legacy_relay_env_vars(env)))
    elif op == "fmt_size":
        size = c["size_mb"] if c["has_size"] else None
        out(op, wtcmd._fmt_size(size))
    elif op == "parse_spawn_tag":
        tag = procid.parse_spawn_tag(c["raw"])
        if tag is None:
            out(op, None)
        else:
            out(op, {
                "install": tag.install,
                "purpose": tag.purpose,
                "spawner_pid": tag.spawner_pid,
                "spawner_create": (float(tag.spawner_create)
                                   if tag.spawner_create is not None else None),
            })
    elif op == "scope_key":
        try:
            class S:
                principal_id = c["principal"]
                transport_family = c["family"]
            out(op, artifacts.artifact_scope_key(S()))
        except Exception:
            out(op, None)
    elif op == "normalize_mime":
        out(op, artifacts._normalize_content_type(c["value"]))
    elif op == "bounded_filename":
        out(op, artifacts._bounded_filename(c["value"]))
    elif op == "proto_supported":
        out(op, broker.browser_control_protocol_supported(c["value"]))
    elif op == "filter_caps":
        dm = c.get("developer_mode")
        caps = broker.filter_browser_control_capabilities(
            c["caps"], developer_mode=dm)
        out(op, sorted(caps))
    elif op == "home_hash":
        out(op, control_socket._home_hash(c["path"]))
    elif op == "socket_paths":
        home = c["path"]
        default = control_socket._default_socket_path(home)
        fallback = control_socket._fallback_socket_path(home)
        bind, pointer = control_socket.resolve_server_socket_path(home)
        out(op, {
            "default_len": len(str(default)),
            "fallback_is_tmp": str(fallback).startswith("/tmp/"),
            "bind_is_default": str(bind) == str(default),
            "has_pointer": pointer is not None,
        })
    elif op == "restart_mechanism":
        out(op, inventory._restart_mechanism(c["supervisor"], c["profile"]))
    elif op == "supervisor_for_pid":
        out(op, inventory._detect_supervisor_for_pid(12345, set()))
    elif op == "route":
        fb = FakeBroker(c.get("scope", 0), c.get("lane", 0),
                        c.get("select", 0), c.get("dispatch_result"))
        try:
            r = router.route_browser_tool(
                "browser_navigate", {"x": 1}, fallback=lambda: "FALLBACK",
                broker=fb, enabled=bool(c.get("enabled")),
                session_id="sess1", task_id="task1",
                principal_id=c.get("principal"),
                transport_family=c.get("family"), tool_call_id="tc1")
        except Exception:
            # ControllerUnavailable (module-local or re-exported via broker)
            out(op, {"status": "ERROR", "result": None})
            continue
        if isinstance(r, str) and r == "FALLBACK":
            out(op, {"status": "FELL_BACK", "result": "FALLBACK"})
        else:
            if not isinstance(r, str):
                r = json.dumps(r, ensure_ascii=False)
            out(op, {"status": "ROUTED", "result": r})
