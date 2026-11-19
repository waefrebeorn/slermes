#!/usr/bin/env python3
"""Generate tests/fixtures/terminal_tool_flow2.jsonl for the terminal_tool_flow2
oracle. Each case is {"func": <name>, "in": {...}} with scripted seams in in.io
(plain value reused; {"__seq__": [...]} per-call; {"__raise__": kind, "msg": text}
raises; scripted null -> None; ABSENT key -> fail-closed RuntimeError). >=5 cases
per ported function, exercising decision branches, short-circuits, error paths,
fail-closed seams, BaseException passthrough and state-mutation order. Integral
floats are avoided in str()-computed positions; embedded NUL and non-mapping
**ctx are outside the fixture domain (see the port header).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "terminal_tool_flow2.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def seq(*vals):
    return {"__seq__": list(vals)}


# ── container_profile ──
add("container_profile", io={"get_hermes_home_override": None, "current_session_profile": "p1"})
add("container_profile", io={"get_hermes_home_override": None, "current_session_profile": "",
                             "home_profile_name": "launch"})
add("container_profile", io={"get_hermes_home_override": "/h/b", "current_session_profile": "p1",
                             "profile_name_for_home": "b"})
add("container_profile", io={"get_hermes_home_override": "/h/b", "current_session_profile": "b",
                             "profile_name_for_home": "b"})
add("container_profile", io={"get_hermes_home_override": "/h/b", "current_session_profile": "",
                             "profile_name_for_home": "b", "is_routed_home": True})
add("container_profile", io={"get_hermes_home_override": "/h/b", "current_session_profile": "",
                             "profile_name_for_home": "b", "is_routed_home": False,
                             "home_profile_name": "launch"})
add("container_profile", io={"get_hermes_home_override": "/h/b", "current_session_profile": "s",
                             "profile_name_for_home": None})
add("container_profile", io={"get_hermes_home_override": "", "current_session_profile": "",
                             "home_profile_name": "launch"})
add("container_profile", io={})
add("container_profile", io={"get_hermes_home_override": R("SystemExit", "bye")})
add("container_profile", io={"get_hermes_home_override": None, "current_session_profile": R("ValueError", "s")})
add("container_profile", io={"get_hermes_home_override": "/h/b", "current_session_profile": "p1",
                             "profile_name_for_home": R("OSError", "p")})
add("container_profile", io={"get_hermes_home_override": "/h/b", "current_session_profile": "",
                             "profile_name_for_home": "b", "is_routed_home": R("KeyboardInterrupt", "ki")})
add("container_profile", io={"get_hermes_home_override": None, "current_session_profile": "",
                             "home_profile_name": R("RuntimeError", "h")})

# ── sanitize_cwd_for_live_env ──
add("sanitize_cwd_for_live_env", env=None, new_cwd="/x", io={"coerce_ssh_remote_cwd": "/x"})
add("sanitize_cwd_for_live_env", env={"env_type": "local"}, new_cwd="/x",
    io={"coerce_ssh_remote_cwd": "/x", "is_container_backend": False})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": "/host/w"}, new_cwd="/host/w/sub",
    io={"coerce_ssh_remote_cwd": "/host/w/sub", "is_container_backend": True, "is_unusable_container_cwd": False})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": "/host/w"}, new_cwd="/host/w",
    io={"coerce_ssh_remote_cwd": "/host/w", "is_container_backend": True,
        "is_unusable_container_cwd": True, "is_mounted_host_cwd": True})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": "/host/w", "host_cwd_mount": "/mnt/w"},
    new_cwd="/host/w",
    io={"coerce_ssh_remote_cwd": "/host/w", "is_container_backend": True,
        "is_unusable_container_cwd": True, "is_mounted_host_cwd": True})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": "/host/w"}, new_cwd="/host/w/sub",
    io={"coerce_ssh_remote_cwd": "/host/w/sub", "is_container_backend": True,
        "is_unusable_container_cwd": True, "is_mounted_host_cwd": False,
        "translate_mounted_host_path": "/mnt/w/sub"})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": "/host/w"}, new_cwd="/host/w/sub",
    io={"coerce_ssh_remote_cwd": "/host/w/sub", "is_container_backend": True,
        "is_unusable_container_cwd": True, "is_mounted_host_cwd": False,
        "translate_mounted_host_path": None})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": None}, new_cwd="/raw",
    io={"coerce_ssh_remote_cwd": "/raw", "is_container_backend": True, "is_unusable_container_cwd": True})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": ""}, new_cwd="/raw",
    io={"coerce_ssh_remote_cwd": "/raw", "is_container_backend": True, "is_unusable_container_cwd": True})
add("sanitize_cwd_for_live_env", env={"env_type": "docker"}, new_cwd="/x",
    io={"coerce_ssh_remote_cwd": R("ValueError", "c")})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": "/h"}, new_cwd="/x",
    io={"coerce_ssh_remote_cwd": "/x", "is_container_backend": True,
        "is_unusable_container_cwd": R("SystemExit", "u")})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": "/h"}, new_cwd="/x",
    io={"coerce_ssh_remote_cwd": "/x", "is_container_backend": True,
        "is_unusable_container_cwd": True, "is_mounted_host_cwd": False,
        "translate_mounted_host_path": R("RuntimeError", "t")})
add("sanitize_cwd_for_live_env", env={"env_type": "docker"}, new_cwd="/x", io={})
add("sanitize_cwd_for_live_env", env={"env_type": "docker", "host_cwd": "/h", "host_cwd_mount": ""},
    new_cwd="/h",
    io={"coerce_ssh_remote_cwd": "/h", "is_container_backend": True,
        "is_unusable_container_cwd": True, "is_mounted_host_cwd": True})
add("sanitize_cwd_for_live_env", env={"env_type": ""}, new_cwd="/x", io={"coerce_ssh_remote_cwd": "/x"})

# ── shell_state_chain ──
add("shell_state_chain", task_id=None)
add("shell_state_chain", task_id="")
add("shell_state_chain", task_id="a", state={"container_aliases": {}})
add("shell_state_chain", task_id="a", state={"container_aliases": {"a": "b", "b": "c"}})
add("shell_state_chain", task_id="a", state={"container_aliases": {"a": "b", "b": "a"}})
add("shell_state_chain", task_id="a", state={"container_aliases": {"a": "a"}})
add("shell_state_chain", task_id="x", state={"container_aliases": {"a": "b"}})
add("shell_state_chain", task_id="a", state={"container_aliases": {"a": "b", "b": "c", "c": "d"}})

# ── session_scope ──
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": "docker", "tenv_bool": True})
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": "local", "tenv_bool": False})
add("session_scope", io={"ensure_terminal_env_bridged": R("RuntimeError", "b")})
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": R("ValueError", "e")})
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": "docker", "tenv_bool": R("SystemExit", "p")})
add("session_scope", io={})
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": "", "tenv_bool": True})

# ── routed_home_task_key ──
add("routed_home_task_key", profile_scoped=True, io={"get_hermes_home_override": None})
add("routed_home_task_key", profile_scoped=True, io={"get_hermes_home_override": ""})
add("routed_home_task_key", profile_scoped=True, io={"get_hermes_home_override": "/h/b", "is_routed_home": False})
add("routed_home_task_key", profile_scoped=True,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "profile_name_for_home": "b", "profile_container_key": "profile:b"})
add("routed_home_task_key", profile_scoped=False,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True, "os.path.realpath": "/real/b"})
add("routed_home_task_key", profile_scoped=True,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "profile_name_for_home": None, "os.path.realpath": "/r"})
add("routed_home_task_key", profile_scoped=True,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "profile_name_for_home": "", "os.path.realpath": "/r"})
add("routed_home_task_key", profile_scoped=False,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "os.path.realpath": R("OSError", "no")})
add("routed_home_task_key", profile_scoped=False,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "os.path.realpath": R("FileNotFoundError", "gone")})
add("routed_home_task_key", profile_scoped=False,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "os.path.realpath": R("SystemExit", "bye")})
add("routed_home_task_key", profile_scoped=False,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "os.path.realpath": R("RuntimeError", "not oserror")})
add("routed_home_task_key", profile_scoped=True,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": R("ValueError", "r")})
add("routed_home_task_key", profile_scoped=True,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "profile_name_for_home": "b", "profile_container_key": R("KeyboardInterrupt", "ki")})
add("routed_home_task_key", profile_scoped=True, io={})
add("routed_home_task_key", profile_scoped=True,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "profile_name_for_home": R("ImportError", "p")})

# ── sessionless_container_key ──
add("sessionless_container_key", profile_scoped=False, io={"get_hermes_home_override": None})
add("sessionless_container_key", profile_scoped=True,
    io={"get_hermes_home_override": None, "home_profile_name": "launch", "profile_container_key": "profile:launch"})
add("sessionless_container_key", profile_scoped=True,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True,
        "profile_name_for_home": "b", "profile_container_key": "profile:b"})
add("sessionless_container_key", profile_scoped=True,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": False,
        "home_profile_name": "", "profile_container_key": ""})
add("sessionless_container_key", profile_scoped=False,
    io={"get_hermes_home_override": "/h/b", "is_routed_home": True, "os.path.realpath": "/r"})
add("sessionless_container_key", profile_scoped=True,
    io={"get_hermes_home_override": None, "home_profile_name": R("RuntimeError", "h")})
add("sessionless_container_key", profile_scoped=True, io={"get_hermes_home_override": R("SystemExit", "bye")})
add("sessionless_container_key", profile_scoped=True, io={})
add("sessionless_container_key", profile_scoped=True,
    io={"get_hermes_home_override": None, "home_profile_name": "p", "profile_container_key": R("ValueError", "k")})

# ── workspace_key_for ──
add("workspace_key_for", cwd="/a/b", io={"os.path.expanduser": "/a/b", "os.path.realpath": "/a/b"})
add("workspace_key_for", cwd="~/w", io={"os.path.expanduser": "/home/u/w", "os.path.realpath": "/home/u/w"})
add("workspace_key_for", cwd="/x", io={"os.path.expanduser": R("OSError", "e")})
add("workspace_key_for", cwd="/x", io={"os.path.expanduser": "/x", "os.path.realpath": R("SystemExit", "r")})
add("workspace_key_for", cwd="/x", io={})
add("workspace_key_for", cwd="", io={"os.path.expanduser": "", "os.path.realpath": ""})
add("workspace_key_for", cwd="/x", io={"os.path.expanduser": "/x", "os.path.realpath": R("ValueError", "r")})

# ── workspace_container_key ──
add("workspace_container_key", base="profile:p", task_id=None, io={})
add("workspace_container_key", base="profile:p", task_id="", io={})
add("workspace_container_key", base="profile:p", task_id="profile:p:ws:abc", io={})
add("workspace_container_key", base="profile:p", task_id="t1", io={"tenv_bool": False})
add("workspace_container_key", base="profile:p", task_id="t1",
    state={"task_env_overrides": {"t1": {}}},
    io={"tenv_bool": True, "resolve_container_alias": "t1", "qualify_task_key": "t1"})
add("workspace_container_key", base="profile:p", task_id="child",
    state={"task_env_overrides": {"q:parent": {"cwd_source": "session", "workspace_key": "d1"}}},
    io={"tenv_bool": True, "resolve_container_alias": "parent", "qualify_task_key": seq("q:child", "q:parent")})
add("workspace_container_key", base="profile:p", task_id="t1",
    state={"task_env_overrides": {"t1": {"cwd_source": "process", "workspace_key": "d"}}},
    io={"tenv_bool": True, "resolve_container_alias": "t1", "qualify_task_key": "t1"})
add("workspace_container_key", base="profile:p", task_id="t1",
    state={"task_env_overrides": {"t1": {"workspace_key": "d"}}},
    io={"tenv_bool": True, "resolve_container_alias": "t1", "qualify_task_key": "t1"})
add("workspace_container_key", base="profile:p", task_id="t1",
    state={"task_env_overrides": {"t1": {"cwd_source": "session", "cwd": "/w"}}},
    io={"tenv_bool": True, "resolve_container_alias": "t1", "qualify_task_key": "t1",
        "os.path.expanduser": "/w", "os.path.realpath": "/w"})
add("workspace_container_key", base="profile:p", task_id="t1",
    state={"task_env_overrides": {"t1": {"cwd_source": "session", "cwd": "   "}}},
    io={"tenv_bool": True, "resolve_container_alias": "t1", "qualify_task_key": "t1"})
add("workspace_container_key", base="profile:p", task_id="t1",
    state={"task_env_overrides": {"t1": {"cwd_source": "session", "cwd": None}}},
    io={"tenv_bool": True, "resolve_container_alias": "t1", "qualify_task_key": "t1"})
add("workspace_container_key", base="profile:p", task_id="t1",
    state={"task_env_overrides": {"t1": {"cwd_source": "session", "workspace_key": "", "cwd": "/w"}}},
    io={"tenv_bool": True, "resolve_container_alias": "t1", "qualify_task_key": "t1",
        "os.path.expanduser": "/w", "os.path.realpath": "/w"})
add("workspace_container_key", base="profile:p", task_id="t1",
    state={"task_env_overrides": {"t1": {"cwd_source": "session", "cwd": "/w"}}},
    io={"tenv_bool": True, "resolve_container_alias": "t1", "qualify_task_key": "t1",
        "os.path.expanduser": R("OSError", "e")})
add("workspace_container_key", base="profile:p", task_id="t1",
    io={"tenv_bool": True, "resolve_container_alias": R("SystemExit", "a")})
add("workspace_container_key", base="profile:p", task_id="t1",
    io={"tenv_bool": True, "resolve_container_alias": "t1", "qualify_task_key": R("RuntimeError", "q")})
add("workspace_container_key", base="profile:p", task_id="t1", io={"tenv_bool": R("ValueError", "b")})
add("workspace_container_key", base="profile:p", task_id="t1", io={})
add("workspace_container_key", base="profile:p", task_id="t1",
    state={"task_env_overrides": {"q:t": {}}},
    io={"tenv_bool": True, "resolve_container_alias": None, "qualify_task_key": seq("q:t", None)})

# ── select_image ──
add("select_image", env_type="docker", overrides={"docker_image": "img1"}, config={"docker_image": "cfg"})
add("select_image", env_type="docker", overrides={}, config={"docker_image": "cfg"})
add("select_image", env_type="docker", overrides={"docker_image": ""}, config={"docker_image": "cfg"})
add("select_image", env_type="docker", overrides={}, config={})
add("select_image", env_type="modal", overrides={"modal_image": "m"}, config={})
add("select_image", env_type="daytona", overrides={}, config={"daytona_image": "d"})
add("select_image", env_type="ssh", overrides={"ssh_image": "s"}, config={})
add("select_image", env_type="ssh", overrides={}, config={})
add("select_image", env_type="local", overrides={"local_image": None}, config={})
add("select_image", env_type="docker", overrides=None, config={"docker_image": "cfg"})
add("select_image", env_type="docker", overrides={}, config=None)
add("select_image", env_type="docker", overrides={"docker_image": 0}, config={"docker_image": "cfg"})
add("select_image", env_type="vercel_sandbox", overrides={"vercel_sandbox_image": "v"}, config={})

# ── lookup_active_env ──
add("lookup_active_env", effective_task_id="e1", task_id=None, io={"hosted_scope": None})
add("lookup_active_env", effective_task_id="e1", task_id=None,
    state={"active_environments": {"e1": {"env_type": "docker"}}},
    io={"hosted_scope": None, "time.time": 100, "admit_env": {"env_type": "docker"}})
add("lookup_active_env", effective_task_id="e1", task_id="t1",
    state={"active_environments": {"t1": {"env_type": "local"}}},
    io={"hosted_scope": None, "time.time": 7, "admit_env": {"env_type": "local"}})
add("lookup_active_env", effective_task_id="e1", task_id="t1",
    state={"active_environments": {"t1": {"x": 1}}}, io={"hosted_scope": {}})
add("lookup_active_env", effective_task_id="", task_id="t1",
    state={"active_environments": {"t1": {"x": 1}}},
    io={"hosted_scope": None, "time.time": 1, "admit_env": {"x": 1}})
add("lookup_active_env", effective_task_id="e1", task_id=None,
    state={"active_environments": {"e1": {"x": 1}}},
    io={"hosted_scope": None, "time.time": R("RuntimeError", "clock")})
add("lookup_active_env", effective_task_id="e1", task_id=None,
    state={"active_environments": {"e1": {"x": 1}}},
    io={"hosted_scope": None, "time.time": 5, "admit_env": R("SystemExit", "bye")})
add("lookup_active_env", effective_task_id="e1", task_id=None,
    state={"active_environments": {"e1": {"x": 1}}},
    io={"hosted_scope": None, "time.time": 5, "admit_env": None})
add("lookup_active_env", effective_task_id="e1", task_id=None, io={"hosted_scope": R("ValueError", "h")})
add("lookup_active_env", effective_task_id="e1", task_id=None, io={})
add("lookup_active_env", effective_task_id="e1", task_id=None,
    state={"active_environments": {"e1": {"x": 1}}},
    io={"hosted_scope": None, "time.time": 5, "admit_env": R("ImportError", "a")})

# ── session_workspace_mount_source ──
add("session_workspace_mount_source", task_id="t", io={"resolve_task_overrides": None})
add("session_workspace_mount_source", task_id="t", io={"resolve_task_overrides": {}})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "/w", "cwd_source": "process"}})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "/w"}, "os.path.expanduser": "/w",
        "os.path.abspath": "/w", "os.path.isdir": True})
add("session_workspace_mount_source", task_id="t", io={"resolve_task_overrides": {"cwd": "   "}})
add("session_workspace_mount_source", task_id="t", io={"resolve_task_overrides": {"cwd": 5}})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "/w"}, "os.path.expanduser": "/w",
        "os.path.abspath": "/w", "os.path.isdir": False})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "/w"}, "os.path.expanduser": "/w",
        "os.path.abspath": "/workspace", "os.path.isdir": True})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "/w"}, "os.path.expanduser": "/w",
        "os.path.abspath": "/workspace/sub", "os.path.isdir": True})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "/w"}, "os.path.expanduser": "/w",
        "os.path.abspath": "/root/x", "os.path.isdir": True})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "/w"}, "os.path.expanduser": R("OSError", "e")})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "/w"}, "os.path.expanduser": "/w",
        "os.path.abspath": R("SystemExit", "a")})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "/w"}, "os.path.expanduser": "/w",
        "os.path.abspath": "/w", "os.path.isdir": R("RuntimeError", "d")})
add("session_workspace_mount_source", task_id="t", io={"resolve_task_overrides": R("ValueError", "o")})
add("session_workspace_mount_source", task_id="t", io={})
add("session_workspace_mount_source", task_id="t",
    io={"resolve_task_overrides": {"cwd": "~/w", "cwd_source": "session"},
        "os.path.expanduser": "/home/u/w", "os.path.abspath": "/home/u/w", "os.path.isdir": True})

# ── backend_default_cwd ──
add("backend_default_cwd", env_type="local", io={"safe_getcwd": "/cur"})
add("backend_default_cwd", env_type="ssh", io={})
add("backend_default_cwd", env_type="vercel_sandbox", io={})
add("backend_default_cwd", env_type="docker", io={"plugin_env_flag": None})
add("backend_default_cwd", env_type="modal", io={"plugin_env_flag": "~"})
add("backend_default_cwd", env_type="daytona", io={"plugin_env_flag": "/sandbox"})
add("backend_default_cwd", env_type="modal", io={"plugin_env_flag": "relative"})
add("backend_default_cwd", env_type="modal", io={"plugin_env_flag": 5})
add("backend_default_cwd", env_type="modal", io={"plugin_env_flag": R("RuntimeError", "p")})
add("backend_default_cwd", env_type="local", io={"safe_getcwd": R("SystemExit", "g")})
add("backend_default_cwd", env_type="docker", io={})
add("backend_default_cwd", env_type="modal", io={"plugin_env_flag": ""})

# ── resolve_config_cwd ──
add("resolve_config_cwd", env_type="local", mount_docker_cwd=False,
    io={"safe_getcwd": "/cur", "tenv": "/cur", "is_container_backend": False})
add("resolve_config_cwd", env_type="local", mount_docker_cwd=False,
    io={"safe_getcwd": "/cur", "tenv": "~/x", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": "/home/u/x", "is_container_backend": False})
add("resolve_config_cwd", env_type="local", mount_docker_cwd=False,
    io={"safe_getcwd": "/cur", "tenv": "~/x", "is_ssh_remote_tilde_cwd": True, "is_container_backend": False})
add("resolve_config_cwd", env_type="local", mount_docker_cwd=False,
    io={"safe_getcwd": "/cur", "tenv": "~/x", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": {}, "is_container_backend": False})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=True,
    io={"plugin_env_flag": None, "tenv": seq("/root", "/host/w"),
        "os.path.expanduser": "/host/w", "os.path.abspath": "/host/w", "is_host_cwd": True})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=True,
    io={"plugin_env_flag": None, "tenv": seq("/root", ""), "safe_getcwd": "/cur",
        "os.path.expanduser": "/cur", "os.path.abspath": "/cur",
        "is_host_cwd": False, "os.path.isabs": True, "os.path.isdir": True})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=True,
    io={"plugin_env_flag": None, "tenv": seq("/root", "/host/w"),
        "os.path.expanduser": "/host/w", "os.path.abspath": "/host/w",
        "is_host_cwd": False, "os.path.isabs": False})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=True,
    io={"plugin_env_flag": None, "tenv": seq("/root", "/host/w"),
        "os.path.expanduser": "/host/w", "os.path.abspath": "/host/w",
        "is_host_cwd": False, "os.path.isabs": True, "os.path.isdir": False})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=True,
    io={"plugin_env_flag": None, "tenv": seq("/root", "/workspace"),
        "os.path.expanduser": "/workspace", "os.path.abspath": "/workspace",
        "is_host_cwd": False, "os.path.isabs": True, "os.path.isdir": True})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=False,
    state={"os_name": "nt"},
    io={"plugin_env_flag": None, "tenv": "C:\\w", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": seq("C:\\w", "C:\\w"),
        "is_windows_drive_path": True, "os.path.abspath": "C:/w", "os.path.isdir": True})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=False,
    io={"plugin_env_flag": None, "tenv": "C:\\w", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": seq("C:\\w", "C:\\w"),
        "is_windows_drive_path": True, "os.path.isdir": False, "logger.info": None})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=False,
    io={"plugin_env_flag": None, "tenv": "/x", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": "/x", "is_windows_drive_path": False})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=False,
    io={"plugin_env_flag": None, "tenv": "/host/x", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": "/host/x", "is_windows_drive_path": False,
        "is_container_backend": True, "is_unusable_container_cwd": True, "logger.info": None})
add("resolve_config_cwd", env_type="modal", mount_docker_cwd=False,
    io={"plugin_env_flag": None, "tenv": "/host/x", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": "/host/x",
        "is_container_backend": True, "is_unusable_container_cwd": True, "logger.info": None})
add("resolve_config_cwd", env_type="modal", mount_docker_cwd=False,
    io={"plugin_env_flag": None, "tenv": "/host/x", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": "/host/x",
        "is_container_backend": True, "is_unusable_container_cwd": False})
add("resolve_config_cwd", env_type="modal", mount_docker_cwd=False,
    io={"plugin_env_flag": None, "tenv": "/host/x", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": "/host/x", "is_container_backend": False})
add("resolve_config_cwd", env_type="ssh", mount_docker_cwd=False,
    io={"tenv": "~", "is_container_backend": False})
add("resolve_config_cwd", env_type="local", mount_docker_cwd=False,
    io={"safe_getcwd": "/cur", "tenv": R("ValueError", "t")})
add("resolve_config_cwd", env_type="local", mount_docker_cwd=False, io={"safe_getcwd": R("SystemExit", "g")})
add("resolve_config_cwd", env_type="local", mount_docker_cwd=False,
    io={"safe_getcwd": "/cur", "tenv": "~/x", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": R("OSError", "e")})
add("resolve_config_cwd", env_type="modal", mount_docker_cwd=False,
    io={"plugin_env_flag": None, "tenv": "/host/x", "is_ssh_remote_tilde_cwd": False,
        "hosted_scope": None, "os.path.expanduser": "/host/x",
        "is_container_backend": True, "is_unusable_container_cwd": True,
        "logger.info": R("RuntimeError", "log")})
add("resolve_config_cwd", env_type="local", mount_docker_cwd=False, io={})
add("resolve_config_cwd", env_type="docker", mount_docker_cwd=True,
    io={"plugin_env_flag": None, "tenv": seq("/root", R("RuntimeError", "t2"))})
add("resolve_config_cwd", env_type="local", mount_docker_cwd=False,
    io={"safe_getcwd": "/cur", "tenv": "~/x", "is_ssh_remote_tilde_cwd": R("KeyboardInterrupt", "ki")})

# ── rewrite_via_env_mount ──
add("rewrite_via_env_mount", path="/h/x", env=None, io={})
add("rewrite_via_env_mount", path="", env={"host_cwd": "/h", "host_cwd_mount": "/m"}, io={})
add("rewrite_via_env_mount", path="/h/x", env={}, io={})
add("rewrite_via_env_mount", path="/h/x", env={"host_cwd": "/h", "host_cwd_mount": "/m"},
    io={"translate_mounted_host_path": "/m/x"})
add("rewrite_via_env_mount", path="/h/x", env={"host_cwd": "", "host_cwd_mount": "/m"}, io={})
add("rewrite_via_env_mount", path="/h/x", env={"host_cwd": 5, "host_cwd_mount": "/m"}, io={})
add("rewrite_via_env_mount", path="/h/x", env={"host_cwd": "/h", "host_cwd_mount": None}, io={})
add("rewrite_via_env_mount", path="/h/x", env={"host_cwd": "/h", "host_cwd_mount": ""}, io={})
add("rewrite_via_env_mount", path="/h/x", env={"host_cwd": "/h", "host_cwd_mount": "/m"},
    io={"translate_mounted_host_path": None})
add("rewrite_via_env_mount", path="/h/x", env={"host_cwd": "/h", "host_cwd_mount": "/m"},
    io={"translate_mounted_host_path": R("SystemExit", "t")})
add("rewrite_via_env_mount", path="/h/x", env={"host_cwd": "/h", "host_cwd_mount": "/m"}, io={})

# ── mount_envs ──
add("mount_envs", env={"a": 1})
add("mount_envs", env=None, state={"active_environments": {}})
add("mount_envs", env=None, state={"active_environments": {"e1": {"x": 1}, "e2": {"y": 2}}})
add("mount_envs", env=None)
add("mount_envs", env="str")
add("mount_envs", env=0)

# ── container_visible_cwd ──
add("container_visible_cwd", path="", env_type="docker", io={})
add("container_visible_cwd", path="/x", env_type=None, io={"is_container_backend": False})
add("container_visible_cwd", path="/x", env_type="local", io={"is_container_backend": False})
add("container_visible_cwd", path="/h/x", env_type="docker",
    env={"host_cwd": "/h", "host_cwd_mount": "/m"},
    io={"is_container_backend": True, "translate_mounted_host_path": "/m/x"})
add("container_visible_cwd", path="/x", env_type="docker", env=None, io={"is_container_backend": True})
add("container_visible_cwd", path="/h/x", env_type="docker", env=None,
    state={"active_environments": {"e1": {"host_cwd": "/h", "host_cwd_mount": "/m"}}},
    io={"is_container_backend": True, "translate_mounted_host_path": "/m/x"})
add("container_visible_cwd", path="/h/x", env_type="docker", env=None,
    state={"active_environments": {"e1": {"x": 1}, "e2": {"host_cwd": "/h", "host_cwd_mount": "/m2"}}},
    io={"is_container_backend": True, "translate_mounted_host_path": "/m2/x"})
add("container_visible_cwd", path="/x", env_type="docker", io={"is_container_backend": R("SystemExit", "c")})
add("container_visible_cwd", path="/h/x", env_type="docker",
    env={"host_cwd": "/h", "host_cwd_mount": "/m"},
    io={"is_container_backend": True, "translate_mounted_host_path": R("RuntimeError", "t")})
add("container_visible_cwd", path="/x", env_type="docker", io={})

# ── container_visible_default ──
add("container_visible_default", default_cwd="/workspace", env_type=None, io={"is_container_backend": False})
add("container_visible_default", default_cwd="/workspace", env_type="docker", env=None,
    io={"is_container_backend": True})
add("container_visible_default", default_cwd="/workspace", env_type="docker",
    env={"host_cwd": "/h", "host_cwd_mount": "/m"}, io={"is_container_backend": True})
add("container_visible_default", default_cwd="/root", env_type="docker",
    env={"host_cwd": "/h", "host_cwd_mount": "/m"},
    io={"is_container_backend": True, "is_unusable_container_cwd": True})
add("container_visible_default", default_cwd="/root", env_type="docker",
    env={"host_cwd": "/h", "host_cwd_mount": "/m"},
    io={"is_container_backend": True, "is_unusable_container_cwd": False})
add("container_visible_default", default_cwd="/root", env_type="docker",
    env={"host_cwd": "/h", "host_cwd_mount": "/root"}, io={"is_container_backend": True})
add("container_visible_default", default_cwd="/workspace", env_type="docker",
    env={"host_cwd_mount": "/m"}, io={"is_container_backend": True})
add("container_visible_default", default_cwd="/workspace", env_type="docker",
    env={"host_cwd": "/h", "host_cwd_mount": ""}, io={"is_container_backend": True})
add("container_visible_default", default_cwd="/opt", env_type="docker",
    env={"host_cwd": "/h", "host_cwd_mount": "/m"},
    io={"is_container_backend": True, "is_unusable_container_cwd": False})
add("container_visible_default", default_cwd="/root", env_type="docker",
    env={"host_cwd": "/h", "host_cwd_mount": "/m"},
    io={"is_container_backend": True, "is_unusable_container_cwd": R("SystemExit", "u")})
add("container_visible_default", default_cwd="/workspace", env_type="docker",
    io={"is_container_backend": R("ValueError", "c")})
add("container_visible_default", default_cwd="/workspace", env_type="docker", io={})
add("container_visible_default", default_cwd="/workspace", env_type="docker", env=None,
    state={"active_environments": {"e1": {"host_cwd": "/h", "host_cwd_mount": "/workspace"},
                                   "e2": {"host_cwd": "/h2", "host_cwd_mount": "/m2"}}},
    io={"is_container_backend": True})

# ── fatal_error_json ──
FIO = {"traceback.format_exc": "TB", "logger.error": None,
       "redact_terminal_error_text": seq("REDACTED", "RTB"), "hosted_scope": None}
add("fatal_error_json", exc=R("RuntimeError", "boom"), io=dict(FIO))
add("fatal_error_json", exc=R("RuntimeError", "boom"),
    io={"traceback.format_exc": "TB", "logger.error": None,
        "redact_terminal_error_text": "REDACTED", "hosted_scope": {}})
add("fatal_error_json", exc=R("KeyError", "boom"), io=dict(FIO))
add("fatal_error_json", exc=R("RuntimeError", "boom"), io={"traceback.format_exc": R("RuntimeError", "tb")})
add("fatal_error_json", exc=R("RuntimeError", "boom"),
    io={"traceback.format_exc": "TB", "logger.error": R("ValueError", "log")})
add("fatal_error_json", exc=R("RuntimeError", "boom"),
    io={"traceback.format_exc": "TB", "logger.error": None,
        "redact_terminal_error_text": R("SystemExit", "r")})
add("fatal_error_json", exc=R("RuntimeError", "boom"),
    io={"traceback.format_exc": "TB", "logger.error": None,
        "redact_terminal_error_text": "REDACTED", "hosted_scope": R("OSError", "h")})
add("fatal_error_json", exc=R("RuntimeError", "boom"),
    io={"traceback.format_exc": "TB", "logger.error": None,
        "redact_terminal_error_text": seq("REDACTED", R("RuntimeError", "r2")), "hosted_scope": None})
add("fatal_error_json", exc=R("RuntimeError", "boom"), io={})
add("fatal_error_json", exc=R("SystemExit", "bye"), io=dict(FIO))
add("fatal_error_json", exc=R("ValueError", "v"),
    io={"traceback.format_exc": "TB", "logger.error": None,
        "redact_terminal_error_text": seq("a\"b\nc", "t"), "hosted_scope": None})

# ── acquire_env ──
PLAN = {"config": {"env_type": "docker"}, "env_type": "docker",
        "effective_task_id": "eff123456789", "image": "img", "cwd": "/w",
        "host_cwd": None, "effective_timeout": 180}
add("acquire_env", plan=dict(PLAN), task_id=None,
    state={"active_environments": {"eff123456789": {"env_type": "docker"}}},
    io={"start_cleanup_thread": None, "hosted_scope": None, "time.time": 5, "admit_env": {"ok": 1}})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None), "time.time": 9,
        "create_configured_env": {"new": 1}, "logger.info": None})
add("acquire_env", plan=dict(PLAN), task_id="t1",
    state={"active_environments": {"t1": {"env_type": "local"}}},
    io={"start_cleanup_thread": None, "hosted_scope": None, "time.time": 5, "admit_env": {"ok": 1}})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None), "logger.info": None,
        "create_configured_env": R("ImportError", "no docker"),
        "redact_terminal_error_text": "R", "error_json": "EJ"})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None), "logger.info": None,
        "create_configured_env": R("ModuleNotFoundError", "no module"),
        "redact_terminal_error_text": "R", "error_json": "EJ"})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None), "logger.info": None,
        "create_configured_env": R("RuntimeError", "boom")})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None), "logger.info": None,
        "create_configured_env": R("SystemExit", "bye")})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None), "logger.info": None,
        "create_configured_env": R("ImportError", "no docker"),
        "redact_terminal_error_text": R("ValueError", "r")})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None), "logger.info": None,
        "create_configured_env": R("ImportError", "no docker"),
        "redact_terminal_error_text": "R", "error_json": R("OSError", "ej")})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None), "logger.info": None,
        "create_configured_env": {"new": 1}, "time.time": R("RuntimeError", "clock")})
add("acquire_env", plan=dict(PLAN), task_id=None, io={"start_cleanup_thread": R("SystemExit", "s")})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None),
        "logger.info": R("RuntimeError", "log1")})
add("acquire_env", plan=dict(PLAN), task_id=None,
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None),
        "create_configured_env": {"new": 1}, "time.time": 9,
        "logger.info": seq(None, R("ValueError", "log2"))})
add("acquire_env", plan=dict(PLAN), task_id=None, io={})
add("acquire_env", plan=dict(PLAN), task_id=None, io={"start_cleanup_thread": None,
                                                      "hosted_scope": R("ImportError", "h")})
add("acquire_env", plan=dict(PLAN), task_id=None,
    state={"creation_locks": {"eff123456789": True}},
    io={"start_cleanup_thread": None, "hosted_scope": seq(None, None), "time.time": 9,
        "create_configured_env": {"new": 1}, "logger.info": None})
add("acquire_env", plan=dict(PLAN), task_id="t1",
    state={"active_environments": {"t1": {"x": 1}}},
    io={"start_cleanup_thread": None, "hosted_scope": {}, "time.time": 5,
        "create_configured_env": {"new": 1}, "logger.info": None})

# ── yield_kwargs ──
CTX = {"env_type": "local", "cwd": "/w", "effective_task_id": "e", "task_id": "t", "session_key": "s"}
add("yield_kwargs", command="c", ctx=dict(CTX), io={"yield_to_background_handler": None})
add("yield_kwargs", command="c", ctx=dict(CTX), io={"yield_to_background_handler": {"fn": 1}})
add("yield_kwargs", command="c", ctx=dict(CTX), io={"yield_to_background_handler": 0})
add("yield_kwargs", command="c", ctx=dict(CTX), io={"yield_to_background_handler": ""})
add("yield_kwargs", command="c", ctx={k: v for k, v in CTX.items() if k != "session_key"}, io={})
add("yield_kwargs", command="c", ctx={k: v for k, v in CTX.items() if k not in ("task_id", "session_key")}, io={})
add("yield_kwargs", command="c",
    ctx={k: v for k, v in CTX.items() if k not in ("cwd", "task_id", "session_key")}, io={})
add("yield_kwargs", command="c",
    ctx={k: v for k, v in CTX.items() if k not in ("cwd", "effective_task_id", "task_id", "session_key")}, io={})
add("yield_kwargs", command="c", ctx=dict(CTX, extra=1), io={})
add("yield_kwargs", command="c", ctx=dict(CTX, command="c2"), io={})
add("yield_kwargs", command="c", ctx={}, io={})
add("yield_kwargs", command="c", ctx=dict(CTX), io={"yield_to_background_handler": R("RuntimeError", "y")})
add("yield_kwargs", command="c", ctx=dict(CTX), io={"yield_to_background_handler": R("SystemExit", "bye")})
add("yield_kwargs", command="c", ctx=dict(CTX), io={})
add("yield_kwargs", command="c", ctx={k: v for k, v in CTX.items() if k != "session_key"} | {"extra": 1}, io={})
add("yield_kwargs", command="c", ctx=dict(CTX, command="c2", extra=1), io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    if short:
        raise SystemExit("funcs with <5 cases: %s" % short)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))


if __name__ == "__main__":
    main()