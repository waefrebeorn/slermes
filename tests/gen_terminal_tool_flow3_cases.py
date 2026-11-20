#!/usr/bin/env python3
"""Generate tests/fixtures/terminal_tool_flow3.jsonl (terminal_tool_flow3 oracle).
Each case is {"func": <name>, "in": {...}} with scripted seams in in.io
(plain value reused; {"__seq__": [...]} per call; {"__raise__": kind, "msg"}
raises; scripted null -> None; ABSENT key -> fail-closed RuntimeError).
>=5 cases per function, covering decision branches, short-circuits, raise and
BaseException passthrough, the full exception hierarchy and state mutation order.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "terminal_tool_flow3.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def seq(*vals):
    return {"__seq__": list(vals)}


# ── command_requires_pipe_stdin (pure) ──
add("command_requires_pipe_stdin", command="gh auth login --with-token")
add("command_requires_pipe_stdin", command="GH AUTH LOGIN   --with-token")
add("command_requires_pipe_stdin", command="  gh\tauth\nlogin  --with-token  ")
add("command_requires_pipe_stdin", command="gh auth login")
add("command_requires_pipe_stdin", command="git auth login --with-token")
add("command_requires_pipe_stdin", command="gh auth login --with-token extra")
add("command_requires_pipe_stdin", command="gh auth login --with-token=x")
add("command_requires_pipe_stdin", command="")
add("command_requires_pipe_stdin", command="GH AUTH LOGIN --WITH-TOKEN")

# ── resolve_notification_flag_conflict (pure) ──
add("resolve_notification_flag_conflict", background=True, notify_on_complete=True, watch_patterns=["x"])
add("resolve_notification_flag_conflict", background=False, notify_on_complete=True, watch_patterns=["x"])
add("resolve_notification_flag_conflict", background=True, notify_on_complete=False, watch_patterns=["x"])
add("resolve_notification_flag_conflict", background=True, notify_on_complete=True, watch_patterns=[])
add("resolve_notification_flag_conflict", background=True, notify_on_complete=True, watch_patterns=None)
add("resolve_notification_flag_conflict", background=True, notify_on_complete=True, watch_patterns="pat")
add("resolve_notification_flag_conflict", background=True, notify_on_complete=True, watch_patterns={})
add("resolve_notification_flag_conflict", background=True, notify_on_complete=True, watch_patterns=[1, 2])

# ── isolation_key (pure leaf) ──
add("isolation_key", key="env_type")
add("isolation_key", key="docker_image")
add("isolation_key", key="modal_image")
add("isolation_key", key="_image")
add("isolation_key", key="image")
add("isolation_key", key="env")
add("isolation_key", key="IMAGE")

# ── session_isolated ──
add("session_isolated", env_type="docker", persistent=False, io={})
add("session_isolated", env_type="docker", persistent=True, io={})
add("session_isolated", env_type="local", persistent=False, io={"plugin_env_flag": True})
add("session_isolated", env_type="local", persistent=True, io={"plugin_env_flag": False})
add("session_isolated", env_type="ssh", persistent=False, io={"plugin_env_flag": seq(False, True)})
add("session_isolated", env_type="", persistent=False, io={"plugin_env_flag": True})
add("session_isolated", env_type="local", persistent=False, io={"plugin_env_flag": R("ValueError", "p")})
add("session_isolated", env_type="local", persistent=False, io={"plugin_env_flag": R("SystemExit", "bye")})

# ── docker_session_isolated ──
add("docker_session_isolated", env_type="docker", persistent=False, io={})
add("docker_session_isolated", env_type="docker", persistent=True, io={})
add("docker_session_isolated", env_type="local", persistent=False, io={"plugin_env_flag": True})
add("docker_session_isolated", env_type="local", persistent=False, io={"plugin_env_flag": False})
add("docker_session_isolated", env_type="", persistent=False, io={"plugin_env_flag": True})
add("docker_session_isolated", env_type="local", persistent=False, io={"plugin_env_flag": R("KeyboardInterrupt", "ki")})

# ── docker_profile_scoped (pure) ──
add("docker_profile_scoped", env_type="docker", persistent=True, io={})
add("docker_profile_scoped", env_type="docker", persistent=False, io={})
add("docker_profile_scoped", env_type="local", persistent=True, io={})
add("docker_profile_scoped", env_type="", persistent=True, io={})
add("docker_profile_scoped", env_type="ssh", persistent=False, io={})

# ── session_scope ──
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": "docker", "tenv_bool": True})
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": "local", "tenv_bool": False})
add("session_scope", io={"ensure_terminal_env_bridged": R("RuntimeError", "bridge"), "tenv": "docker", "tenv_bool": True})
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": R("ValueError", "t"), "tenv_bool": True})
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": "docker", "tenv_bool": R("SystemExit", "s")})
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": None, "tenv_bool": None})
add("session_scope", io={"ensure_terminal_env_bridged": None, "tenv": "ssh", "tenv_bool": False})

# ── docker_session_isolation_enabled ──
BASE_SCOPE = {"ensure_terminal_env_bridged": None}
add("docker_session_isolation_enabled", io=dict(BASE_SCOPE, tenv="docker", tenv_bool=False))
add("docker_session_isolation_enabled", io=dict(BASE_SCOPE, tenv="docker", tenv_bool=True))
add("docker_session_isolation_enabled", io=dict(BASE_SCOPE, tenv="local", tenv_bool=False, plugin_env_flag=True))
add("docker_session_isolation_enabled", io=dict(BASE_SCOPE, tenv="local", tenv_bool=False, plugin_env_flag=False))
add("docker_session_isolation_enabled", io=dict(BASE_SCOPE, tenv="local", tenv_bool=False, plugin_env_flag=R("RuntimeError", "p")))
add("docker_session_isolation_enabled", io=dict(BASE_SCOPE, tenv="docker", tenv_bool=False, ensure_terminal_env_bridged=R("OSError", "b")))
add("docker_session_isolation_enabled", io=dict(BASE_SCOPE, tenv="docker", tenv_bool=R("KeyboardInterrupt", "k")))

# ── has_isolation_overrides ──
add("has_isolation_overrides", task_id=None, io={})
add("has_isolation_overrides", task_id="t", state={"task_env_overrides": {"t": {"docker_image": "x"}}},
    io={"qualify_task_key": "t"})
add("has_isolation_overrides", task_id="t", state={"task_env_overrides": {"t": {"env_type": "docker"}}},
    io={"qualify_task_key": "t"})
add("has_isolation_overrides", task_id="t", state={"task_env_overrides": {"t": {"foo": 1}}},
    io={"qualify_task_key": "t"})
add("has_isolation_overrides", task_id="t", state={"task_env_overrides": {}}, io={"qualify_task_key": "t"})
add("has_isolation_overrides", task_id="t", state={"task_env_overrides": {"t": {"a": 1, "b_image": "y"}}},
    io={"qualify_task_key": "t"})
add("has_isolation_overrides", task_id="t", io={"qualify_task_key": R("RuntimeError", "q")})
add("has_isolation_overrides", task_id="t", io={"qualify_task_key": R("SystemExit", "bye")})
add("has_isolation_overrides", task_id="t", io={"qualify_task_key": seq("a", "b")})

# ── register_container_alias ──
add("register_container_alias", child="", parent="p", io={})
add("register_container_alias", child="c", parent="p", io={})
add("register_container_alias", child="c", parent=None, io={})
add("register_container_alias", child="c", parent="", io={})
add("register_container_alias", child="c", parent=5, io={})
add("register_container_alias", child="c", parent=False, io={})
add("register_container_alias", child="c", parent=True, io={})
add("register_container_alias", child="c", parent="p", state={"container_aliases": {"old": "x"}}, io={})

# ── maybe_reap_docker_orphans ──
REAP_IO = {"import_docker": None, "tenv": "300", "container_identity": "prof", "reap_orphan_containers": 2}
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO))
add("maybe_reap_docker_orphans", config={"docker_orphan_reaper": False}, io=dict(REAP_IO))
add("maybe_reap_docker_orphans", config={}, state={"docker_orphan_reaper_ran": True}, io=dict(REAP_IO))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, tenv="0"))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, tenv="abc"))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, tenv=R("ValueError", "bad")))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, tenv=R("SystemExit", "bye")))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, import_docker=R("ModuleNotFoundError", "import of tools.environments.docker halted; None in sys.modules")))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, container_identity=R("RuntimeError", "ci")))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, reap_orphan_containers=R("ValueError", "reap")))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, reap_orphan_containers=R("SystemExit", "bye")))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, reap_orphan_containers=0))
add("maybe_reap_docker_orphans", config={}, io=dict(REAP_IO, reap_orphan_containers=3,
    **{"logger.info": R("RuntimeError", "log")}))
add("maybe_reap_docker_orphans", config={"docker_shared_container_key": "shared"}, io=dict(REAP_IO))

# ── refuse_missing_local_workdir ──
add("refuse_missing_local_workdir", workdir="/w", env={}, io={})
add("refuse_missing_local_workdir", workdir="/w", env={"__local_env__": True}, io={"usable_cwd": ["/w", False]})
add("refuse_missing_local_workdir", workdir="/w", env={"__local_env__": True}, io={"usable_cwd": ["/w", "/f"], "error_json": "err-json"})
add("refuse_missing_local_workdir", workdir="/x", env={"__local_env__": True}, io={"usable_cwd": ["/x", 1], "error_json": "j"})
add("refuse_missing_local_workdir", workdir="/w", env={"__local_env__": True}, io={"usable_cwd": R("ValueError", "u")})
add("refuse_missing_local_workdir", workdir="/w", env={"__local_env__": True}, io={"usable_cwd": ["/w", "/f"], "error_json": R("RuntimeError", "e")})
add("refuse_missing_local_workdir", workdir="/w", env={"__local_env__": True}, io={"usable_cwd": ["/w", "/f"], "error_json": R("SystemExit", "bye")})

# ── resolve_command_cwd ──
CW = {"get_session_cwd": None, "is_container_backend": False,
      "container_visible_default": "/def", "coerce_ssh_remote_cwd": "/final"}
add("resolve_command_cwd", workdir="/w", io=dict(CW, container_visible_cwd="/vis"))
add("resolve_command_cwd", workdir="", io=CW)
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r"))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=True,
    is_unusable_container_cwd=False))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=True,
    is_unusable_container_cwd=True, in_sandbox_home=True))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=True,
    is_unusable_container_cwd=True, in_sandbox_home=False, container_visible_cwd="/v"))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=True,
    is_unusable_container_cwd=True, in_sandbox_home=False, container_visible_cwd="/r",
    is_mounted_host_cwd=True))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=True,
    is_unusable_container_cwd=True, in_sandbox_home=False, container_visible_cwd="/r",
    is_mounted_host_cwd=False))
add("resolve_command_cwd", io=dict(CW, get_session_cwd=R("RuntimeError", "g")))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=R("ValueError", "b")))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=True,
    is_unusable_container_cwd=R("TypeError", "u")))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=True,
    is_unusable_container_cwd=True, in_sandbox_home=R("SystemExit", "bye")))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=True,
    is_unusable_container_cwd=True, in_sandbox_home=False, container_visible_cwd="/r",
    is_mounted_host_cwd=R("RuntimeError", "m")))
add("resolve_command_cwd", io=dict(CW, get_session_cwd="/r", is_container_backend=True,
    is_unusable_container_cwd=True, in_sandbox_home=False, container_visible_cwd="/r",
    is_mounted_host_cwd=True, **{"logger.info": R("RuntimeError", "log")}))

# ── resolve_task_host_cwd ──
HOST = {"docker_session_isolation_enabled": False, "resolve_container_task_id": "default",
        "container_profile": "p", "profile_container_key": "p",
        "resolve_container_alias": "c", "session_workspace_mount_source": "/src",
        "workspace_key_for": "h"}
add("resolve_task_host_cwd", config={"env_type": "local", "docker_mount_cwd_to_workspace": True}, task_id="t", io={})
add("resolve_task_host_cwd", config={"env_type": "docker"}, task_id="t", io={})
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": False}, task_id="t", io={})
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True, "host_cwd": "/host"},
    task_id="t", io=dict(HOST))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True},
    task_id="t", io=dict(HOST, docker_session_isolation_enabled=True, resolve_container_task_id="p:ws:h"))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True, "host_cwd": "/host"},
    task_id="t", io=dict(HOST, session_workspace_mount_source=None))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True, "host_cwd": "/host"},
    task_id="t", io=dict(HOST, resolve_container_task_id="p:ws:other"))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True},
    task_id="t", io=dict(HOST, docker_session_isolation_enabled=True, resolve_container_task_id=seq("default", "p:ws:h")))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True},
    task_id="t", io=dict(HOST, docker_session_isolation_enabled=R("RuntimeError", "d")))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True},
    task_id="t", io=dict(HOST, resolve_container_task_id=R("ValueError", "r")))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True},
    task_id="t", io=dict(HOST, container_profile=R("OSError", "c")))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True},
    task_id="t", io=dict(HOST, session_workspace_mount_source=R("SystemExit", "bye")))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True},
    task_id="t", io=dict(HOST, workspace_key_for=R("RuntimeError", "w")))
add("resolve_task_host_cwd", config={"env_type": "docker", "docker_mount_cwd_to_workspace": True},
    task_id=None, io=dict(HOST))


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
