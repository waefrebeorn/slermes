#!/usr/bin/env python3
"""Differential oracle for hermes_platform/host/runtime.py (gate w24_runtime_d45154).

The live module is imported and every predicate is called with its decisions
fed as values: the process environment is replaced by a patched ``os.environ``,
``os.path.exists`` answers from the fixture, ``_read_proc`` returns the fixture
text and ``open`` (the /proc/version read) serves a StringIO.  Nothing on this
machine's real /proc or real environment is consulted, and the memo globals are
reset per case so no earlier answer can leak into a later one.
"""
import io
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
GATE = "w24_runtime_d45154"
PYFILE = QUARRY / "hermes_platform/host/runtime.py"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The module imports only ``os``, so its file is loaded directly: the host
# package __init__ stays out of the way and the code compared is the live file.
_spec = importlib.util.spec_from_file_location("live_host_runtime", PYFILE)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

CGROUP_MARKERS = ("docker", "podman", "/lxc/", "kubepods", "containerd", "crio")
ROOT_MARKERS = ("kubepods", "containerd", "crio")

WANTED = {"termux", "wsl", "proc_marker", "root_mount", "container"}


def _env_of(r):
    env = r.get("env")
    if not isinstance(env, dict):
        return {}
    return {str(k): str(v) for k, v in env.items() if isinstance(v, str)}


def termux(r):
    with mock.patch.dict(os.environ, _env_of(r), clear=True):
        return live.is_termux()


def wsl(r):
    proc = r.get("proc")
    text = proc.get("proc_version") if isinstance(proc, dict) else None
    live._wsl_detected = None

    def _boom(*a, **k):
        raise OSError("unreadable /proc/version")

    try:
        if not isinstance(text, str):
            # the OSError arm of is_wsl: an unreadable /proc/version is False
            with mock.patch("builtins.open", _boom):
                return live.is_wsl()
        with mock.patch("builtins.open", lambda *a, **k: io.StringIO(text)):
            return live.is_wsl()
    finally:
        live._wsl_detected = None


def _content_of(r):
    proc = r.get("proc")
    if not isinstance(proc, dict):
        return ""
    text = proc.get("content")
    return text if isinstance(text, str) else ""


def _markers_of(r, default):
    proc = r.get("proc")
    markers = proc.get("markers") if isinstance(proc, dict) else None
    return tuple(markers) if isinstance(markers, list) else tuple(default)


def proc_marker(r):
    content = _content_of(r)
    markers = _markers_of(r, CGROUP_MARKERS)
    with mock.patch.object(live, "_read_proc", lambda p: content):
        return live._proc_file_has_marker("/proc/1/cgroup", markers)


def root_mount(r):
    content = _content_of(r)
    markers = _markers_of(r, ROOT_MARKERS)
    with mock.patch.object(live, "_read_proc", lambda p: content):
        return live._root_mount_has_marker("/proc/self/mountinfo", markers)


def container(r):
    env = r.get("env") if isinstance(r.get("env"), dict) else {}
    files = {}
    if isinstance(env.get("cgroup"), str):
        files["/proc/1/cgroup"] = env["cgroup"]
    if isinstance(env.get("mountinfo"), str):
        files["/proc/self/mountinfo"] = env["mountinfo"]
    exists = set()
    if env.get("dockerenv"):
        exists.add("/.dockerenv")
    if env.get("containerenv"):
        exists.add("/run/.containerenv")
    environ = {}
    if env.get("k8s_host"):
        environ["KUBERNETES_SERVICE_HOST"] = str(env["k8s_host"])
    live._container_detected = None
    try:
        with mock.patch.dict(os.environ, environ, clear=True), \
                mock.patch.object(live.os.path, "exists", lambda p: p in exists), \
                mock.patch.object(live, "_read_proc", lambda p: files.get(p, "")):
            return live.is_container()
    finally:
        live._container_detected = None


DISPATCH = {
    "termux": termux,
    "wsl": wsl,
    "proc_marker": proc_marker,
    "root_mount": root_mount,
    "container": container,
}


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
            py = DISPATCH[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
