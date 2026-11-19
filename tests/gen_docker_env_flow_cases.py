#!/usr/bin/env python3
"""Generate tests/fixtures/docker_env_flow.jsonl for the docker_env_flow gate.

Cases are {func, in}; the C harness computes out, the oracle recomputes the
LIVE Python reference from in. io scripts follow the universal-seam contract:
single value reused, bare array = per-call queue (last repeats),
{"__script__": true, "ret": [...]} = constant list result. Log seams are left
unscripted (void -> null) unless a case probes a raise.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "docker_env_flow.jsonl")

CID = "abc123def456789012"
CID12 = CID[:12]
IMG = "hermes-sandbox:latest"
HEX = "abcdef0123456789abcdef0123456789"


def cap(rc, out="", err=""):
    return {"returncode": rc, "stdout": out, "stderr": err}


def raise_spec(kind, msg):
    return {"__raise__": kind, "msg": msg}


def S(ret):
    return {"__script__": True, "ret": ret}


def self_attach(**kw):
    d = {"_docker_exe": "docker", "_image": IMG, "_image_pinned": False, "_container_id": None}
    d.update(kw)
    return d


def self_run(**kw):
    d = {"_docker_exe": "docker", "_image": "img:1", "_image_uses_s6_init": False,
         "_snap_compat": False,
         "_labels": {"hermes-agent": "1", "hermes-task-id": "default",
                     "hermes-profile": "default", "hermes-egress": "off"},
         "_all_run_args": ["--cap-drop", "ALL"], "_run_env_values": {}}
    d.update(kw)
    return d


def self_mount(**kw):
    d = {"_persistent": False, "_sandbox_mounts": [], "_workspace_dir": None, "_home_dir": None}
    d.update(kw)
    return d


CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── docker_query ──
add("docker_query", argv=["docker", "ps"], timeout=15, fail="ps failed: %s",
    io={"run_capture": cap(0, "id1\nid2\n")})
add("docker_query", argv=["docker", "ps"], timeout=15, fail="ps failed: %s",
    io={"run_capture": cap(1, "", "boom")})
add("docker_query", argv=["docker", "ps"], timeout=15, fail="ps failed: %s",
    nonzero="ps returned %d: %s", io={"run_capture": cap(1, "", " boom \n")})
add("docker_query", argv=["docker", "ps"], timeout=15, fail="ps failed: %s",
    io={"run_capture": raise_spec("TimeoutExpired", "timed out")})
add("docker_query", argv=["docker", "ps"], timeout=15, fail="ps failed: %s",
    io={"run_capture": raise_spec("OSError", "no cli")})
add("docker_query", argv=["docker", "ps"], timeout=15, fail="ps failed: %s",
    io={"run_capture": raise_spec("CalledProcessError", "exit 1")})
add("docker_query", argv=["docker", "ps"], timeout=15, fail="ps failed: %s",
    exc_types="subprocess", io={"run_capture": raise_spec("CalledProcessError", "exit 1")})
add("docker_query", argv=["docker", "rm", CID], timeout=30,
    fail="rm %s failed: %s", fail_args=[CID12], nonzero="rm %s returned %d: %s",
    io={"run_capture": cap(2, "", "x")})
add("docker_query", argv=["docker", "ps"], timeout=15, fail="ps failed: %s",
    io={"run_capture": raise_spec("SystemExit", "bye")})

# ── is_idle_workspace_container ──
add("is_idle_workspace_container", docker_exe="docker", container_id=CID,
    io={"run_capture": cap(0, "0123456789abcdef\n")})
add("is_idle_workspace_container", docker_exe="docker", container_id=CID,
    io={"run_capture": cap(0, "0123456789abcde")})
add("is_idle_workspace_container", docker_exe="docker", container_id=CID,
    io={"run_capture": cap(0, "0123456789ABCDEF")})
add("is_idle_workspace_container", docker_exe="docker", container_id=CID,
    io={"run_capture": cap(0, "  0123456789abcdef  ")})
add("is_idle_workspace_container", docker_exe="docker", container_id=CID,
    io={"run_capture": cap(1, "", "err")})
add("is_idle_workspace_container", docker_exe="docker", container_id=CID,
    io={"run_capture": raise_spec("TimeoutExpired", "slow")})
add("is_idle_workspace_container", docker_exe="docker", container_id=CID,
    io={"run_capture": cap(0, "0123456789abcdefx")})
add("is_idle_workspace_container", docker_exe="docker", container_id=CID,
    io={"run_capture": cap(0, "")})

# ── extra_args_network_mode ──
add("extra_args_network_mode", extra_args=None)
add("extra_args_network_mode", extra_args=[])
add("extra_args_network_mode", extra_args=["--network", "none"])
add("extra_args_network_mode", extra_args=["--network=host"])
add("extra_args_network_mode", extra_args=["--net", "bridge"])
add("extra_args_network_mode", extra_args=["--net=none"])
add("extra_args_network_mode", extra_args=["--network"])
add("extra_args_network_mode", extra_args=["--network", 5, "host"])
add("extra_args_network_mode", extra_args=["--net", "--network=host"])
add("extra_args_network_mode", extra_args=[5, "--shm-size=1g"])
add("extra_args_network_mode", extra_args=["--network=", "x"])
add("extra_args_network_mode", extra_args=["--netx", "--net=host"])

# ── split_volume_spec ──
add("split_volume_spec", spec="/h:/c")
add("split_volume_spec", spec="C:/h:/c:ro")
add("split_volume_spec", spec="  /h:/c  ")
add("split_volume_spec", spec="a:/b:")
add("split_volume_spec", spec="a:/")
add("split_volume_spec", spec="a:b")
add("split_volume_spec", spec=None)
add("split_volume_spec", spec=5)
add("split_volume_spec", spec="host:/c:ro:extra")
add("split_volume_spec", spec="/a:/b:/c:ro")
add("split_volume_spec", spec="a\nb:/c")
add("split_volume_spec", spec="x:/y:z")
add("split_volume_spec", spec="\u00a0/h:/c\u00a0")

# ── container_mount_taken ──
add("container_mount_taken", volume_args=["-v", "/h:/workspace"], mount="/workspace")
add("container_mount_taken", volume_args=["-v", "/h:/workspace/"], mount="/workspace/")
add("container_mount_taken", volume_args=["-v", "h://:ro"], mount="")
add("container_mount_taken", volume_args=["-v", "/h:/other"], mount="/workspace")
add("container_mount_taken", volume_args=["-v", "--mount"], mount="/workspace")
add("container_mount_taken", volume_args=["-v", "/h:/workspace:ro"], mount="/workspace")
add("container_mount_taken", volume_args=[], mount="/workspace")

# ── existing_host_mount ──
add("existing_host_mount", volume_args=["-v", "C:/Proj:/workspace"], host_cwd_abs="c:/proj",
    io={"host_path_key": ["c:/proj", "c:/proj"]})
add("existing_host_mount", volume_args=["-v", "/h:/c"], host_cwd_abs="",
    io={"host_path_key": ""})
add("existing_host_mount", volume_args=["-v", "/h:/c"], host_cwd_abs="c:/proj",
    io={"host_path_key": ["c:/proj", "d:/other"]})
add("existing_host_mount", volume_args=["-v", "/h:/c"], host_cwd_abs="c:/proj",
    io={"host_path_key": ["c:/proj", "c:/h"]})
add("existing_host_mount", volume_args=["-v", "D:/Data:/data"], host_cwd_abs="d:/data/",
    io={"host_path_key": ["d:/data", "d:/data"]})
add("existing_host_mount", volume_args=[], host_cwd_abs="c:/proj",
    io={"host_path_key": "c:/proj"})
add("existing_host_mount", volume_args=[], host_cwd_abs="c:/proj",
    io={"host_path_key": raise_spec("OSError", "bad path")})

# ── free_host_cwd_mount ──
add("free_host_cwd_mount", volume_args=[])
add("free_host_cwd_mount", volume_args=["-v", "/h:/host-cwd"])
add("free_host_cwd_mount", volume_args=["-v", "/h:/host-cwd", "-v", "/h2:/host-cwd-2"])
add("free_host_cwd_mount", volume_args=["-v", "/h:/host-cwd", "-v", "/h2:/host-cwd-2",
                                        "-v", "/h3:/host-cwd-3"])
add("free_host_cwd_mount", volume_args=["-v", "/h:/other"])
add("free_host_cwd_mount", volume_args=["-v", "/h:/host-cwd-2/"])

# ── abs_host_cwd ──
add("abs_host_cwd", host_cwd="proj", os_name="posix",
    io={"expanduser": "proj", "is_windows_drive_path": False, "abspath": "/home/u/proj"})
add("abs_host_cwd", host_cwd="~", os_name="posix",
    io={"expanduser": "/home/u", "is_windows_drive_path": False, "abspath": "/home/u"})
add("abs_host_cwd", host_cwd="C:/proj", os_name="posix",
    io={"expanduser": "C:/proj", "is_windows_drive_path": True})
add("abs_host_cwd", host_cwd="C:/proj", os_name="nt",
    io={"expanduser": "C:/proj", "is_windows_drive_path": True, "abspath": "C:\\proj"})
add("abs_host_cwd", host_cwd="/abs/path", os_name="posix",
    io={"expanduser": "/abs/path", "is_windows_drive_path": False, "abspath": "/abs/path"})
add("abs_host_cwd", host_cwd="proj", os_name="posix",
    io={"expanduser": raise_spec("OSError", "no home")})

# ── host_user_args ──
add("host_user_args", run_as_host_user=False)
add("host_user_args", run_as_host_user=True, io={"resolve_host_user_spec": "1000:1000"})
add("host_user_args", run_as_host_user=True, io={"resolve_host_user_spec": None})
add("host_user_args", run_as_host_user=True, io={"resolve_host_user_spec": ""})
add("host_user_args", run_as_host_user=True,
    io={"resolve_host_user_spec": raise_spec("OSError", "no ids")})

# ── attach_existing_container ──
ATT = dict(task_label="default", profile_name="default", egress_label="off")
add("attach_existing_container", **ATT, network=True, self=self_attach(),
    io={"find_reusable_container": None})
add("attach_existing_container", **ATT, network=True, self=self_attach(),
    io={"find_reusable_container": S([CID, "running"]), "run_capture": cap(0, IMG + "\n")})
add("attach_existing_container", **ATT, network=True, self=self_attach(),
    io={"find_reusable_container": S([CID, "running"]), "run_capture": cap(0, "other:1")})
add("attach_existing_container", **ATT, network=True, self=self_attach(_image_pinned=True),
    io={"find_reusable_container": S([CID, "running"]),
        "run_capture": [cap(0, "other:1"), cap(1, "", "no pull"), cap(1, "", "pull fail")]})
add("attach_existing_container", **ATT, network=True, self=self_attach(_image_pinned=True),
    io={"find_reusable_container": S([CID, "running"]),
        "run_capture": [cap(0, "other:1"), cap(1, "", ""), cap(0, "pulled"), cap(0, "")]})
add("attach_existing_container", **ATT, network=True, self=self_attach(_image_pinned=True),
    io={"find_reusable_container": S([CID, "running"]),
        "run_capture": [cap(0, "other:1"), cap(0, "sha"),
                        raise_spec("TimeoutExpired", "rm slow")]})
add("attach_existing_container", **ATT, network=False, self=self_attach(),
    io={"find_reusable_container": S([CID, "running"]),
        "run_capture": [cap(0, IMG), cap(0, "")], "container_network_mode": "bridge"})
add("attach_existing_container", **ATT, network=False, self=self_attach(),
    io={"find_reusable_container": S([CID, "running"]),
        "run_capture": [cap(0, IMG), cap(0, "")], "container_network_mode": None})
add("attach_existing_container", **ATT, network=False, self=self_attach(),
    io={"find_reusable_container": S([CID, "exited"]),
        "run_capture": [cap(0, IMG), cap(0, "")], "container_network_mode": "none"})
add("attach_existing_container", **ATT, network=True, self=self_attach(),
    io={"find_reusable_container": S([CID, "exited"]),
        "run_capture": [cap(0, IMG), raise_spec("CalledProcessError", "exit 125")]})
add("attach_existing_container", **ATT, network=True, self=self_attach(),
    io={"find_reusable_container": S([CID, "running"]),
        "run_capture": cap(1, "", "daemon down")})
add("attach_existing_container", **ATT, network=True, self=self_attach(),
    io={"find_reusable_container": S([CID, "exited"]),
        "run_capture": [cap(0, IMG), raise_spec("OSError", "no cli")]})
add("attach_existing_container", **ATT, network=False, self=self_attach(),
    io={"find_reusable_container": S([CID, "running"]),
        "run_capture": [cap(0, IMG), raise_spec("OSError", "perm")],
        "container_network_mode": "bridge"})
add("attach_existing_container", **ATT, network=True, self=self_attach(),
    io={"find_reusable_container": S([CID, "running"]), "run_capture": cap(0, "")})

# ── image_available_locally ──
add("image_available_locally", self=self_attach(), io={"run_capture": cap(0, "sha256:abc")})
add("image_available_locally", self=self_attach(),
    io={"run_capture": [cap(1, "", ""), cap(0, "", "")]})
add("image_available_locally", self=self_attach(),
    io={"run_capture": [cap(1, "", ""), cap(1, "", "manifest")]})
add("image_available_locally", self=self_attach(),
    io={"run_capture": raise_spec("TimeoutExpired", "probe slow")})
add("image_available_locally", self=self_attach(),
    io={"run_capture": [cap(1, "", ""), raise_spec("OSError", "net")]})
add("image_available_locally", self=self_attach(),
    io={"run_capture": raise_spec("SystemExit", "stop")})
add("image_available_locally", self=self_attach(),
    io={"run_capture": raise_spec("CalledProcessError", "exit 1")})

# ── start_container ──
add("start_container", container_id=CID, self=self_attach(), io={"run_capture": cap(0)})
add("start_container", container_id=CID, self=self_attach(),
    io={"run_capture": raise_spec("CalledProcessError", "exit 1")})
add("start_container", container_id=CID, self=self_attach(),
    io={"run_capture": raise_spec("TimeoutExpired", "timed out after 30s")})
add("start_container", container_id=CID, self=self_attach(),
    io={"run_capture": raise_spec("OSError", "no cli")})
add("start_container", container_id=CID, self=self_attach(),
    io={"run_capture": raise_spec("SystemExit", "x")})
add("start_container", container_id=CID, self=self_attach(),
    io={"run_capture": raise_spec("SubprocessError", "generic")})

# ── docker_run ──
add("docker_run", cwd="/root", self=self_run(),
    io={"uuid4_hex": HEX, "run_capture": cap(0, "  newcid1234567890  \n"),
        "client_env_with": None})
add("docker_run", cwd="/root", self=self_run(_run_env_values={"TOKEN": "x"}),
    io={"uuid4_hex": HEX, "run_capture": cap(0, "cid2"),
        "client_env_with": {"PATH": "/usr/bin", "TOKEN": "x"}})
add("docker_run", cwd="/root", self=self_run(),
    io={"uuid4_hex": HEX, "client_env_with": None,
        "run_capture": raise_spec("CalledProcessError", "exit 125")})
add("docker_run", cwd="/root", self=self_run(),
    io={"uuid4_hex": HEX, "client_env_with": None,
        "run_capture": raise_spec("TimeoutExpired", "pull slow")})
add("docker_run", cwd="/root", self=self_run(),
    io={"uuid4_hex": HEX, "client_env_with": None,
        "run_capture": raise_spec("OSError", "no cli")})
add("docker_run", cwd="/root", self=self_run(),
    io={"uuid4_hex": HEX, "client_env_with": None,
        "run_capture": raise_spec("CalledProcessError", "exit 125"),
        "subprocess_run": raise_spec("OSError", "rm failed")})
add("docker_run", cwd="/root", self=self_run(_image_uses_s6_init=True),
    io={"uuid4_hex": HEX, "run_capture": cap(0, "cid3"), "client_env_with": None})
add("docker_run", cwd="/root", self=self_run(_snap_compat=True),
    io={"uuid4_hex": HEX, "run_capture": cap(0, "cid4"), "client_env_with": None})
add("docker_run", cwd="/root", self=self_run(),
    io={"uuid4_hex": "abc", "run_capture": cap(0, "cid5"), "client_env_with": None})

# ── run_command ──
add("run_command", name="hermes-x", workdir="/root", self=self_run())
add("run_command", name="hermes-x", workdir="/root", self=self_run(_image_uses_s6_init=True))
add("run_command", name="hermes-x", workdir="/root", self=self_run(_snap_compat=True))
add("run_command", name="hermes-x", workdir="/root",
    self=self_run(_image_uses_s6_init=True, _snap_compat=True))
add("run_command", name="hermes-x", workdir="/root", self=self_run(_labels={}))
add("run_command", name="hermes-x", workdir="/root", self=self_run(_labels={"z": "1", "a": "2"}))
add("run_command", name="hermes-x", workdir="/work",
    self=self_run(_all_run_args=["--cap-drop", "ALL", "--tmpfs", "/tmp:rw,size=512m"]))

# ── resource_args ──
add("resource_args", self={}, image=IMG, cpu=0.5, memory=512, disk=0, network=True,
    shm_size="1g", extra_args=None, platform="linux",
    io={"cgroup_limits_available": True, "extra_args_set_shm_size": False})
add("resource_args", self={}, image=IMG, cpu=0.5, memory=512, disk=0, network=True,
    shm_size="1g", extra_args=None, platform="linux",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": False})
add("resource_args", self={}, image=IMG, cpu=2.0, memory=0, disk=0, network=True,
    shm_size="1g", extra_args=None, platform="linux",
    io={"cgroup_limits_available": True, "extra_args_set_shm_size": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=0, network=True,
    shm_size="", extra_args=None, platform="linux", io={"cgroup_limits_available": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=0, network=True,
    shm_size="0", extra_args=None, platform="linux", io={"cgroup_limits_available": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=0, network=True,
    shm_size="1g", extra_args=["--shm-size=2g"], platform="linux",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": True})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=100, network=True,
    shm_size="1g", extra_args=None, platform="linux",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": False,
        "storage_opt_supported": True})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=100, network=True,
    shm_size="1g", extra_args=None, platform="linux",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": False,
        "storage_opt_supported": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=100, network=True,
    shm_size="1g", extra_args=None, platform="darwin",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=0, network=False,
    shm_size="1g", extra_args=None, platform="linux",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=0, network=False,
    shm_size="1g", extra_args=["--network=host"], platform="linux",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=0, network=False,
    shm_size="1g", extra_args=["--net", "none"], platform="linux",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=0, network=True,
    shm_size=" 2g ", extra_args=None, platform="linux",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=0, network=True,
    shm_size="1g", extra_args=None, platform="linux",
    io={"cgroup_limits_available": True, "extra_args_set_shm_size": False})
add("resource_args", self={}, image=IMG, cpu=0, memory=0, disk=0, network=False,
    shm_size="1g", extra_args=["--network=it's"], platform="linux",
    io={"cgroup_limits_available": False, "extra_args_set_shm_size": False})

# ── mount_args ──
add("mount_args", self=self_mount(), volumes=None, host_cwd=None, auto_mount_cwd=False,
    task_id="default", os_name="posix", io={"is_windows_drive_path": False})
add("mount_args", self=self_mount(_persistent=True), volumes=None, host_cwd=None,
    auto_mount_cwd=False, task_id="default", os_name="posix",
    io={"is_windows_drive_path": False, "get_sandbox_dir": "/sandboxes",
        "sanitize_task_id_for_path": "default"})
add("mount_args", self=self_mount(), volumes=["/h:/workspace"], host_cwd=None,
    auto_mount_cwd=False, task_id="default", os_name="posix",
    io={"is_windows_drive_path": False})
add("mount_args", self=self_mount(), volumes=None, host_cwd="/proj", auto_mount_cwd=True,
    task_id="default", os_name="posix",
    io={"expanduser": "/proj", "abspath": "/proj", "is_windows_drive_path": False,
        "isdir": True, "host_path_key": "/proj"})
add("mount_args", self=self_mount(), volumes=["/h:/workspace"], host_cwd="/proj",
    auto_mount_cwd=True, task_id="default", os_name="posix",
    io={"expanduser": "/proj", "abspath": "/proj", "is_windows_drive_path": False,
        "isdir": True, "host_path_key": ["/proj", "/h"]})
add("mount_args", self=self_mount(), volumes=["C:/proj:/workspace"], host_cwd="c:/proj",
    auto_mount_cwd=True, task_id="default", os_name="posix",
    io={"expanduser": "c:/proj", "abspath": "c:/proj", "is_windows_drive_path": False,
        "isdir": True, "host_path_key": ["c:/proj", "c:/proj"]})
add("mount_args", self=self_mount(), volumes=None, host_cwd="D:/proj", auto_mount_cwd=False,
    task_id="default", os_name="posix",
    io={"expanduser": "D:/proj", "is_windows_drive_path": True, "isdir": True,
        "host_path_key": "d:/proj"})
add("mount_args", self=self_mount(), volumes=None, host_cwd="/missing", auto_mount_cwd=True,
    task_id="default", os_name="posix",
    io={"expanduser": "/missing", "abspath": "/missing", "is_windows_drive_path": False,
        "isdir": False})
add("mount_args", self=self_mount(), volumes=[5, "  ", "nocolon", "/h:/c"], host_cwd=None,
    auto_mount_cwd=False, task_id="default", os_name="posix",
    io={"is_windows_drive_path": False})
add("mount_args", self=self_mount(_persistent=True), volumes=["/h:/workspace"], host_cwd=None,
    auto_mount_cwd=False, task_id="default", os_name="posix",
    io={"is_windows_drive_path": False, "get_sandbox_dir": "/sandboxes",
        "sanitize_task_id_for_path": "default"})
add("mount_args", self=self_mount(), volumes=None, host_cwd="~/proj", auto_mount_cwd=True,
    task_id="default", os_name="posix",
    io={"expanduser": "/home/u/proj", "abspath": "/home/u/proj",
        "is_windows_drive_path": False, "isdir": True, "host_path_key": "/home/u/proj"})
add("mount_args", self=self_mount(),
    volumes=["/h:/workspace", "/h2:/host-cwd", "/h3:/host-cwd-2", "/h4:/host-cwd-3"],
    host_cwd="/proj", auto_mount_cwd=True, task_id="default", os_name="posix",
    io={"expanduser": "/proj", "abspath": "/proj", "is_windows_drive_path": False,
        "isdir": True, "host_path_key": ["/proj", "/h", "/h2", "/h3", "/h4"]})
add("mount_args", self=self_mount(_persistent=True), volumes=None, host_cwd=None,
    auto_mount_cwd=False, task_id="default", os_name="posix",
    io={"is_windows_drive_path": False, "get_sandbox_dir": "/sandboxes",
        "sanitize_task_id_for_path": "default",
        "makedirs": raise_spec("OSError", "perm")})
add("mount_args", self=self_mount(_persistent=True), volumes=None, host_cwd=None,
    auto_mount_cwd=False, task_id="sess:ws:0123456789abcdef", os_name="posix",
    io={"is_windows_drive_path": False, "get_sandbox_dir": "/sandboxes/",
        "sanitize_task_id_for_path": "sess_ws_01234567-abcdef"})

# ── egress_and_env_args ──
EG0 = dict(egress_proxy_args_for_docker=S([[], {}, []]),
           egress_reuse_fingerprint="off", critical_egress_env_names=S([]))
add("egress_and_env_args", self={"_forward_env": [], "_env": {}}, extra_args=None,
    io=dict(EG0, merge_egress_env={"A": "1"}))
add("egress_and_env_args", self={"_forward_env": [], "_env": {}}, extra_args=None,
    io={"egress_proxy_args_for_docker": S([["-v", "/ca:/ca:ro"],
                                          {"HTTPS_PROXY": "http://p"},
                                          ["--add-host", "host.docker.internal:host-gateway"]]),
        "egress_reuse_fingerprint": "eg123", "egress_enforce_on_docker": False,
        "critical_egress_env_names": S(["HTTPS_PROXY"]),
        "merge_egress_env": {"HTTPS_PROXY": "http://p", "B": "2"}})
add("egress_and_env_args", self={"_forward_env": ["TOKEN"], "_env": {}}, extra_args=None,
    io={"egress_proxy_args_for_docker": S([[], {"HTTPS_PROXY": "p"}, []]),
        "egress_reuse_fingerprint": "eg1", "egress_enforce_on_docker": True,
        "critical_egress_env_names": S(["HTTPS_PROXY"]),
        "check_forward_env_collisions": raise_spec("RuntimeError", "collision")})
add("egress_and_env_args", self={"_forward_env": [], "_env": {"HTTPS_PROXY": "x"}},
    extra_args=None,
    io={"egress_proxy_args_for_docker": S([[], {"HTTPS_PROXY": "p"}, []]),
        "egress_reuse_fingerprint": "eg1", "egress_enforce_on_docker": True,
        "critical_egress_env_names": S(["HTTPS_PROXY"]),
        "check_docker_env_collisions": raise_spec("RuntimeError", "env collision")})
add("egress_and_env_args", self={"_forward_env": [], "_env": {}},
    extra_args=["--env=HTTPS_PROXY=x"],
    io={"egress_proxy_args_for_docker": S([[], {"HTTPS_PROXY": "p"}, []]),
        "egress_reuse_fingerprint": "eg1", "egress_enforce_on_docker": True,
        "critical_egress_env_names": S(["HTTPS_PROXY"]),
        "merge_egress_env": {"HTTPS_PROXY": "p"},
        "check_extra_args_collisions": raise_spec("RuntimeError", "extra collision")})
add("egress_and_env_args", self={"_forward_env": [], "_env": {}},
    extra_args=["--ok", 5, None, "--yes"],
    io=dict(EG0, merge_egress_env={}))
add("egress_and_env_args", self={"_forward_env": [], "_env": {}}, extra_args=None,
    io=dict(EG0, merge_egress_env={"Z": "1", "A": "2", "M": "3"}))
add("egress_and_env_args", self={"_forward_env": ["TOKEN"], "_env": {"K": "v"}},
    extra_args=None,
    io={"egress_proxy_args_for_docker": S([[], {"HTTPS_PROXY": "p"}, []]),
        "egress_reuse_fingerprint": "eg1", "egress_enforce_on_docker": True,
        "critical_egress_env_names": S(["HTTPS_PROXY"]),
        "merge_egress_env": {"K": "v", "HTTPS_PROXY": "p"}})
add("egress_and_env_args", self={"_forward_env": [], "_env": {}}, extra_args=None,
    io={"egress_proxy_args_for_docker": None})

# ── container_image ──
add("container_image", container_id=CID, self=self_attach(), io={"run_capture": cap(0, "img:1\n")})
add("container_image", container_id=CID, self=self_attach(), io={"run_capture": cap(0, "")})
add("container_image", container_id=CID, self=self_attach(), io={"run_capture": cap(0, "   ")})
add("container_image", container_id=CID, self=self_attach(), io={"run_capture": cap(1, "", "err")})
add("container_image", container_id=CID, self=self_attach(),
    io={"run_capture": raise_spec("TimeoutExpired", "slow")})
add("container_image", container_id=CID, self=self_attach(),
    io={"run_capture": raise_spec("OSError", "no cli")})
add("container_image", container_id=CID, self=self_attach(), io={"run_capture": cap(0, "  img:2  ")})
add("container_image", container_id=CID, self=self_attach(),
    io={"run_capture": cap(0, "\u00a0img:3\u00a0")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-32s %d" % (k, counts[k]))
    thin = [k for k, v in counts.items() if v < 5]
    if thin:
        raise SystemExit("funcs with <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
