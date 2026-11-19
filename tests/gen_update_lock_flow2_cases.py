#!/usr/bin/env python3
"""Generate tests/fixtures/update_lock_flow2.jsonl.

Inputs only (no expected values): the oracle recomputes the live reference.
Scripted seam results live in in.io (see the harness header). Paths are POSIX
strings; bytes travel as UTF-8 strings; u64 past 2^53 use {"__bigint__": dec}.
A bare JSON array in in.io is a call-QUEUE; a seam whose live result is itself a
list is wrapped in script(ret=[...]). sys.platform is a case input (in.platform).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "update_lock_flow2.jsonl")
CASES = []
W = {"pid": 100, "ct": 1000.0, "now": 2000.0}
PATH = "/h/.hermes-update-in-progress"


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_spec=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    return s


def marker(raw="200\n1999\nct:1999\n", pid=200, started_at=1999, create_time=1999.0,
           delegate_pid=None, delegate_create_time=None, in_flight=False,
           ct_text="1999", delegate_ct_text=None, runs=()):
    return {"raw": raw, "pid": pid, "started_at": started_at, "create_time": create_time,
            "delegate_pid": delegate_pid, "delegate_create_time": delegate_create_time,
            "in_flight": in_flight, "ct_text": ct_text, "delegate_ct_text": delegate_ct_text,
            "runs": list(runs)}


def selfspec(**over):
    s = {"path": PATH, "install_root": None, "_checkout": False, "acquired": False, "holder": None}
    s.update(over)
    return s


def heldspec(**over):
    h = {"path": "/install/.git/hermes-update.lock", "fd": 5, "owned": True, "depth": 1}
    h.update(over)
    return h


KB = {"type": "KeyboardInterrupt", "message": "interrupted"}
RT = {"type": "RuntimeError", "message": "boom"}
OSERR = {"type": "OSError", "message": "io fail", "errno": 5}
FNF = {"type": "FileNotFoundError", "message": "gone"}
PERM = {"type": "PermissionError", "message": "denied", "errno": 13}

# ── _git_common_dir ─────────────────────────────────────────────────────────
add("_git_common_dir", root="/install", io={"path.is_dir": True, "path.is_file": False})
add("_git_common_dir", root="/install",
    io={"path.is_dir": False, "path.is_file": [True, False], "path.read_text": "gitdir: /real/.git\n"})
add("_git_common_dir", root="/install",
    io={"path.is_dir": False, "path.is_file": [True, True],
        "path.read_text": ["gitdir: /real/.git/worktrees/wt", "../.."]})
add("_git_common_dir", root="/install", io={"path.is_dir": False, "path.is_file": False})
add("_git_common_dir", root="/install",
    io={"path.is_dir": False, "path.is_file": True, "path.read_text": "not a gitdir"})
add("_git_common_dir", root="/install", io={"path.is_dir": script(raise_spec=OSERR)})
add("_git_common_dir", root="/install", io={"path.is_dir": script(raise_spec=KB)})
add("_git_common_dir", root="/install",
    io={"path.is_dir": False, "path.is_file": [True, True],
        "path.read_text": ["gitdir: /real/.git", script(raise_spec=OSERR)]})

# ── checkout_lock_path ──────────────────────────────────────────────────────
add("checkout_lock_path", install_root="/install", io={"_git_common_dir": None})
add("checkout_lock_path", install_root="/install", io={"_git_common_dir": "/install/.git"})
add("checkout_lock_path", install_root=None,
    io={"_default_install_root": "/def", "_git_common_dir": None})
add("checkout_lock_path", install_root="",
    io={"_default_install_root": "/def", "_git_common_dir": "/def/.git"})
add("checkout_lock_path", install_root="/install", io={"_git_common_dir": script(raise_spec=RT)})
add("checkout_lock_path", install_root=None, io={"_default_install_root": script(raise_spec=RT)})

# ── incarnation_live ────────────────────────────────────────────────────────
IL_IO = {"_as_ct": 1999.0, "_real_world": W, "_incarnation": True}
add("incarnation_live", pid=200, recorded_ct="ct:1999", io=IL_IO)
add("incarnation_live", pid="200", recorded_ct=1999.0, io=IL_IO)
add("incarnation_live", pid="abc", recorded_ct=None, io={})
add("incarnation_live", pid=None, recorded_ct=None, io={})
add("incarnation_live", pid=12.7, recorded_ct=None, io={**IL_IO, "_incarnation": None})
add("incarnation_live", pid=0, recorded_ct=None, io={**IL_IO, "_incarnation": False})
add("incarnation_live", pid=200, recorded_ct=None, io={**IL_IO, "_incarnation": script(raise_spec=RT)})
add("incarnation_live", pid=200, recorded_ct=None, io={**IL_IO, "_as_ct": script(raise_spec=KB)})
add("incarnation_live", pid=True, recorded_ct=None, io=IL_IO)

# ── _real_world ─────────────────────────────────────────────────────────────
RW_IO = {"os.getpid": 100, "_own_create_time": 1000.0, "time.time": 2000.0}
add("_real_world", io=RW_IO)
add("_real_world", io={**RW_IO, "_own_create_time": None})
add("_real_world", io={**RW_IO, "os.getpid": script(raise_spec=RT)})
add("_real_world", io={**RW_IO, "time.time": script(raise_spec=RT)})
add("_real_world", io={**RW_IO, "_own_create_time": script(raise_spec=KB)})
add("_real_world", io={**RW_IO, "os.getpid": 4242, "_own_create_time": 5.5, "time.time": 9.25})

# ── _identity_line ──────────────────────────────────────────────────────────
add("_identity_line", pid=None, io={"process_create_time": 1700000000.123456})
add("_identity_line", pid=200, io={"process_create_time": None})
add("_identity_line", pid=None, io={"process_create_time": 1000.0})
add("_identity_line", pid=None, io={"process_create_time": 0.5})
add("_identity_line", pid=None, io={"process_create_time": 1700000000.9999})
add("_identity_line", pid=None, io={"process_create_time": script(raise_spec=RT)})
add("_identity_line", pid=None, io={"process_create_time": 2.5})

# ── _is_runtime_host ────────────────────────────────────────────────────────
add("_is_runtime_host", cmdline=["hermes", "gateway", "run"],
    io={"looks_like_gateway_command_line": True})
add("_is_runtime_host", cmdline=["hermes", "serve"],
    io={"looks_like_gateway_command_line": False, "_hermes_holder_subcommand": "serve"})
add("_is_runtime_host", cmdline=["hermes", "dashboard"],
    io={"looks_like_gateway_command_line": False, "_hermes_holder_subcommand": "dashboard"})
add("_is_runtime_host", cmdline=["hermes", "update"],
    io={"looks_like_gateway_command_line": False, "_hermes_holder_subcommand": "other"})
add("_is_runtime_host", cmdline=[],
    io={"looks_like_gateway_command_line": False, "_hermes_holder_subcommand": None})
add("_is_runtime_host", cmdline=["x"],
    io={"looks_like_gateway_command_line": script(raise_spec=RT)})
add("_is_runtime_host", cmdline=["x"],
    io={"looks_like_gateway_command_line": False, "_hermes_holder_subcommand": script(raise_spec=KB)})

# ── _read_bytes ─────────────────────────────────────────────────────────────
add("_read_bytes", path=PATH, io={"path.read_bytes": "200\n1999\n"})
add("_read_bytes", path=PATH, io={"path.read_bytes": ""})
add("_read_bytes", path=PATH, io={"path.read_bytes": script(raise_spec=OSERR)})
add("_read_bytes", path=PATH, io={"path.read_bytes": script(raise_spec=PERM)})
add("_read_bytes", path=PATH, io={"path.read_bytes": script(raise_spec=KB)})
add("_read_bytes", path=PATH, io={"path.read_bytes": script(raise_spec=RT)})

# ── _read_marker ────────────────────────────────────────────────────────────
add("_read_marker", path=PATH,
    io={"_read_bytes": "200\n1999\nct:1999\n", "_parse_marker": marker()})
add("_read_marker", path=PATH, io={"_read_bytes": None})
add("_read_marker", path=PATH,
    io={"_read_bytes": "", "path.stat": {"st_mtime": 1995.0}, "_parse_marker": marker(in_flight=True)})
add("_read_marker", path=PATH,
    io={"_read_bytes": "", "path.stat": script(raise_spec=OSERR), "_parse_marker": marker()})
add("_read_marker", path=PATH,
    io={"_read_bytes": "", "path.stat": script(raise_spec=RT)})
add("_read_marker", path=PATH,
    io={"_read_bytes": "200\n1999\n", "_parse_marker": script(raise_spec=RT)})
add("_read_marker", path=PATH, io={"_read_bytes": script(raise_spec=KB)})

# ── _tmp_sibling ────────────────────────────────────────────────────────────
add("_tmp_sibling", path=PATH, io={"os.getpid": 100, "secrets.token_hex": "abcd1234"})
add("_tmp_sibling", path="/a/b/.marker", io={"os.getpid": 7, "secrets.token_hex": "00ff"})
add("_tmp_sibling", path=PATH, io={"os.getpid": script(raise_spec=RT)})
add("_tmp_sibling", path=PATH, io={"os.getpid": 100, "secrets.token_hex": script(raise_spec=KB)})
add("_tmp_sibling", path="/.marker", io={"os.getpid": 1, "secrets.token_hex": "zz"})
add("_tmp_sibling", path=PATH, io={"os.getpid": 999999, "secrets.token_hex": "deadbeef"})

# ── _tmp_writer_gone ────────────────────────────────────────────────────────
add("_tmp_writer_gone", owner=None, entry=PATH, io={})
add("_tmp_writer_gone", owner=200, entry=PATH, io={"os.getpid": 100, "_pid_alive": True})
add("_tmp_writer_gone", owner=200, entry=PATH, io={"os.getpid": 100, "_pid_alive": False})
add("_tmp_writer_gone", owner=100, entry=PATH,
    io={"os.getpid": 100, "time.time": 2000.0, "path.stat": {"st_mtime": 1900.0}})
add("_tmp_writer_gone", owner=100, entry=PATH,
    io={"os.getpid": 100, "time.time": 2000.0, "path.stat": {"st_mtime": 1990.0}})
add("_tmp_writer_gone", owner=200, entry=PATH, io={"os.getpid": 100, "_pid_alive": script(raise_spec=RT)})
add("_tmp_writer_gone", owner=100, entry=PATH,
    io={"os.getpid": 100, "time.time": 2000.0, "path.stat": script(raise_spec=OSERR)})

# ── marker_mutex_path ───────────────────────────────────────────────────────
add("marker_mutex_path", path=PATH, io={})
add("marker_mutex_path", path="/a/b/.marker", io={})
add("marker_mutex_path", path="/.marker", io={})
add("marker_mutex_path", path=".marker", io={})
add("marker_mutex_path", path="/h/.hermes-update-in-progress", io={})

# ── _sweep_dead_tmp_siblings ────────────────────────────────────────────────
SW = "/h/.marker"
add("_sweep_dead_tmp_siblings", path=SW, io={"path.iterdir": script([])})
add("_sweep_dead_tmp_siblings", path=SW,
    io={"path.iterdir": script(["/h/.marker.200.abcd.tmp"]), "_bounded_int": 200,
        "_tmp_writer_gone": True, "path.unlink": script(None)})
add("_sweep_dead_tmp_siblings", path=SW,
    io={"path.iterdir": script(["/h/other.200.tmp"]), "_bounded_int": 200})
add("_sweep_dead_tmp_siblings", path=SW,
    io={"path.iterdir": script(["/h/.marker.abc.tmp"]), "_bounded_int": None})
add("_sweep_dead_tmp_siblings", path=SW,
    io={"path.iterdir": script(["/h/.marker.200.abcd.tmp"]), "_bounded_int": 200,
        "_tmp_writer_gone": False})
add("_sweep_dead_tmp_siblings", path=SW, io={"path.iterdir": script(raise_spec=OSERR)})
add("_sweep_dead_tmp_siblings", path=SW,
    io={"path.iterdir": script(["/h/.marker.200.a.tmp", "/h/.marker.300.b.tmp"]),
        "_bounded_int": [200, 300], "_tmp_writer_gone": [script(raise_spec=OSERR), True],
        "path.unlink": script(None)})
add("_sweep_dead_tmp_siblings", path=SW,
    io={"path.iterdir": script(["/h/.marker.200.a.tmp"]), "_bounded_int": script(raise_spec=RT)})
add("_sweep_dead_tmp_siblings", path=SW,
    io={"path.iterdir": script(["/h/.marker.99999999999.a.tmp"]), "_bounded_int": None,
        "_tmp_writer_gone": True, "path.unlink": script(None)})

# ── _compare_and_swap ───────────────────────────────────────────────────────
def cas_io(**over):
    io = {"mutex.enter": script(None), "mutex.exit": script(None),
          "_read_bytes": "old", "_tmp_sibling": "/h/.marker.100.t.tmp",
          "tmp.write": script(None), "os.replace": script(None), "path.unlink": script(None)}
    io.update(over)
    return io


add("_compare_and_swap", path=PATH, expected="old", new="new", io=cas_io())
add("_compare_and_swap", path=PATH, expected="different", new="new", io=cas_io())
add("_compare_and_swap", path=PATH, expected="old", new="new",
    io=cas_io(**{"tmp.write": script(raise_spec=OSERR)}))
add("_compare_and_swap", path=PATH, expected="old", new="new",
    io=cas_io(**{"os.replace": script(raise_spec=OSERR)}))
add("_compare_and_swap", path=PATH, expected="old", new="new",
    io=cas_io(**{"tmp.write": script(raise_spec=RT)}))
add("_compare_and_swap", path=PATH, expected="old", new="new",
    io=cas_io(**{"mutex.enter": script(raise_spec={"type": "MarkerBusy", "message": "held"})}))
add("_compare_and_swap", path=PATH, expected="old", new="new",
    io=cas_io(**{"path.unlink": script(raise_spec=OSERR)}))
add("_compare_and_swap", path=PATH, expected="old", new="new",
    io=cas_io(**{"_read_bytes": script(raise_spec=RT)}))

# ── legacy_profile_claims ───────────────────────────────────────────────────
add("legacy_profile_claims", root="/h",
    io={"path.iterdir": script(["/h/profiles/a"]), "_read_marker": marker(),
        "marker.live_pid": 200, "marker.age": 5.0})
add("legacy_profile_claims", root="/h",
    io={"path.iterdir": script(["/h/profiles/a"]), "_read_marker": None})
add("legacy_profile_claims", root="/h",
    io={"path.iterdir": script(["/h/profiles/a"]), "_read_marker": marker(in_flight=True)})
add("legacy_profile_claims", root="/h",
    io={"path.iterdir": script(["/h/profiles/a"]), "_read_marker": marker(), "marker.live_pid": 0})
add("legacy_profile_claims", root="/h", io={"path.iterdir": script(raise_spec=OSERR)})
add("legacy_profile_claims", root=None,
    io={"update_marker_path": "/h/.hermes-update-in-progress",
        "path.iterdir": script(["/h/profiles/a"]), "_read_marker": marker(),
        "marker.live_pid": 200, "marker.age": 5.0})
add("legacy_profile_claims", root="/h",
    io={"path.iterdir": script(["/h/profiles/a", "/h/profiles/b"]),
        "_read_marker": [marker(), script(raise_spec=OSERR)],
        "marker.live_pid": 200, "marker.age": 5.0})
add("legacy_profile_claims", root="/h",
    io={"path.iterdir": script(["/h/profiles/a"]), "_read_marker": marker(),
        "marker.live_pid": script(raise_spec=RT)})

# ── _lock_holder ────────────────────────────────────────────────────────────
add("_lock_holder", path=PATH,
    io={"_read_bytes": "200\n1999\nct:1999\n", "_parse_marker": marker(),
        "marker.age": 5.0, "incarnation_live": False})
add("_lock_holder", path=PATH,
    io={"_read_bytes": "200\n1999\nct:1999\n", "_parse_marker": marker(),
        "marker.age": 5.0, "incarnation_live": True})
add("_lock_holder", path=PATH,
    io={"_read_bytes": "200\n1999\nct:1999\n", "_parse_marker": marker(),
        "marker.age": 5.0, "incarnation_live": None})
add("_lock_holder", path=PATH,
    io={"_read_bytes": "x\n", "_parse_marker": marker(pid=-1, started_at=None, create_time=None, ct_text=None, raw="x\n")})
add("_lock_holder", path=PATH,
    io={"_read_bytes": None, "_parse_marker": marker(started_at=None, create_time=None, ct_text=None, raw="")})
add("_lock_holder", path=PATH,
    io={"_read_bytes": "200\n1999\n", "_parse_marker": marker(create_time=None, ct_text=None, raw="200\n1999\n"),
        "marker.age": 5.0, "incarnation_live": script(raise_spec=RT)})
add("_lock_holder", path=PATH,
    io={"_read_bytes": "0\n1999\n", "_parse_marker": marker(pid=0, raw="0\n1999\n"), "marker.age": 5.0})

# ── _lock_with_contention_wait ──────────────────────────────────────────────
add("_lock_with_contention_wait", fd=5, path=PATH, io={"_try_lock": True})
add("_lock_with_contention_wait", fd=5, path=PATH,
    io={"_try_lock": False, "_held_by_our_windows_ancestor": True})
add("_lock_with_contention_wait", fd=5, path=PATH,
    io={"_try_lock": [False, False], "_held_by_our_windows_ancestor": False,
        "time.monotonic": [0, 0.5, 4], "time.sleep": [None]})
add("_lock_with_contention_wait", fd=5, path=PATH,
    io={"_try_lock": [False, True], "_held_by_our_windows_ancestor": False,
        "time.monotonic": [0, 0.5], "time.sleep": [None]})
add("_lock_with_contention_wait", fd=5, path=PATH, io={"_try_lock": script(raise_spec=RT)})
add("_lock_with_contention_wait", fd=5, path=PATH,
    io={"_try_lock": False, "_held_by_our_windows_ancestor": False, "time.monotonic": script(raise_spec=KB)})
add("_lock_with_contention_wait", fd=5, path=PATH,
    io={"_try_lock": [False, False, False], "_held_by_our_windows_ancestor": False,
        "time.monotonic": [0, 0.5, 1.5, 4], "time.sleep": [None, None]})

# ── _held_by_our_windows_ancestor ───────────────────────────────────────────
add("_held_by_our_windows_ancestor", path=PATH, platform="linux", io={})
add("_held_by_our_windows_ancestor", path=PATH, platform="win32",
    io={"_read_bytes": "x\n", "_parse_marker": marker(started_at=None, pid=-1, raw="x\n")})
add("_held_by_our_windows_ancestor", path=PATH, platform="win32",
    io={"_read_bytes": "100\n1999\n", "_parse_marker": marker(pid=100, raw="100\n1999\n"), "os.getpid": 100})
add("_held_by_our_windows_ancestor", path=PATH, platform="win32",
    io={"_read_bytes": "200\n1999\nct:1999\n", "_parse_marker": marker(), "os.getpid": 100,
        "incarnation_live": True, "_is_ancestor_pid": True})
add("_held_by_our_windows_ancestor", path=PATH, platform="win32",
    io={"_read_bytes": "200\n1999\nct:1999\n", "_parse_marker": marker(), "os.getpid": 100,
        "incarnation_live": False})
add("_held_by_our_windows_ancestor", path=PATH, platform="win32",
    io={"_read_bytes": "200\n1999\nct:1999\n", "_parse_marker": marker(), "os.getpid": 100,
        "incarnation_live": True, "_is_ancestor_pid": False})
add("_held_by_our_windows_ancestor", path=PATH, platform="win32",
    io={"_read_bytes": "200\n1999\nct:1999\n", "_parse_marker": marker(), "os.getpid": 100,
        "incarnation_live": None})

# ── _release_checkout ───────────────────────────────────────────────────────
add("_release_checkout", held=None, io={})
add("_release_checkout", held=heldspec(depth=2), io={})
add("_release_checkout", held=heldspec(depth=1, owned=True, fd=5),
    io={"_unlock": script(None), "os.close": script(None)})
add("_release_checkout", held=heldspec(depth=1, owned=False, fd=None, lease=[7, 1048577]),
    io={"_unlock": script(None), "os.close": script(None)})
add("_release_checkout", held=heldspec(depth=1, owned=True, fd=5, lease=[7, 1048577]),
    io={"_unlock": script(None), "os.close": script(None)})
add("_release_checkout", held=heldspec(depth=1, owned=True, fd=5),
    io={"_unlock": script(None), "os.close": script(raise_spec=OSERR)})
add("_release_checkout", held=heldspec(depth=1, owned=True, fd=5),
    io={"_unlock": script(None), "os.close": script(raise_spec=RT)})
add("_release_checkout", held=heldspec(depth=1, owned=True, fd=5),
    io={"_unlock": script(raise_spec=RT)})

# ── checkout_lock_fds ───────────────────────────────────────────────────────
add("checkout_lock_fds", install_root="/install", platform="win32", held=heldspec(), io={})
add("checkout_lock_fds", install_root="/install", platform="linux", held=heldspec(fd=5), io={})
add("checkout_lock_fds", install_root="/install", platform="linux", held=heldspec(fd=None), io={})
add("checkout_lock_fds", install_root="/install", platform="linux", held=None,
    io={"checkout_lock_path": "/install/.git/hermes-update.lock", "_inherited_lock_fd": 9})
add("checkout_lock_fds", install_root="/install", platform="linux", held=None,
    io={"checkout_lock_path": "/install/.git/hermes-update.lock", "_inherited_lock_fd": None})
add("checkout_lock_fds", install_root="/install", platform="linux", held=None,
    io={"checkout_lock_path": script(raise_spec=RT)})

# ── custody_spawn_kwargs ────────────────────────────────────────────────────
add("custody_spawn_kwargs", platform="win32", held=heldspec(), io={})
add("custody_spawn_kwargs", platform="linux", held=None, io={})
add("custody_spawn_kwargs", platform="linux", held=heldspec(fd=None), io={})
add("custody_spawn_kwargs", platform="linux", held=heldspec(fd=5), io={})
add("custody_spawn_kwargs", platform="linux", held=heldspec(fd=9), io={})

# ── holds_checkout_lock ─────────────────────────────────────────────────────
add("holds_checkout_lock", install_root="/install", held=None, io={})
add("holds_checkout_lock", install_root="/install", held=heldspec(path="/x"),
    io={"os.path.realpath": ["/x", "/x"], "checkout_lock_path": "/x"})
add("holds_checkout_lock", install_root="/install", held=heldspec(path="/x"),
    io={"os.path.realpath": ["/x", "/y"], "checkout_lock_path": "/y"})
add("holds_checkout_lock", install_root="/install", held=heldspec(path="/x"),
    io={"os.path.realpath": script(raise_spec=RT)})
add("holds_checkout_lock", install_root="/install", held=heldspec(path="/x"),
    io={"os.path.realpath": "/x", "checkout_lock_path": script(raise_spec=KB)})
add("holds_checkout_lock", install_root=None, held=heldspec(path="/x"),
    io={"os.path.realpath": ["/x", "/x"], "checkout_lock_path": "/x"})

# ── update_in_progress ──────────────────────────────────────────────────────
add("update_in_progress", install_root="/install",
    io={"read_live_update": {"pid": 200, "age_seconds": 5.0}})
add("update_in_progress", install_root="/install",
    io={"read_live_update": None, "checkout_lock_held": True})
add("update_in_progress", install_root="/install",
    io={"read_live_update": None, "checkout_lock_held": False})
add("update_in_progress", install_root="/install",
    io={"read_live_update": script(raise_spec=RT)})
add("update_in_progress", install_root="/install",
    io={"read_live_update": None, "checkout_lock_held": script(raise_spec=KB)})
add("update_in_progress", install_root=None,
    io={"read_live_update": {"pid": 0, "age_seconds": 0.0, "held": True}})

# ── checkout_lock_held ──────────────────────────────────────────────────────
def clh_io(**over):
    io = {"holds_checkout_lock": False, "checkout_lock_path": "/install/.git/hermes-update.lock",
          "os.open": 5, "_try_shared_lock": True, "os.close": script(None)}
    io.update(over)
    return io


add("checkout_lock_held", install_root="/install", io=clh_io(holds_checkout_lock=True))
add("checkout_lock_held", install_root="/install", io=clh_io(**{"os.open": script(raise_spec=FNF)}))
add("checkout_lock_held", install_root="/install", io=clh_io(**{"os.open": script(raise_spec=OSERR)}))
add("checkout_lock_held", install_root="/install", io=clh_io(_try_shared_lock=True))
add("checkout_lock_held", install_root="/install", io=clh_io(_try_shared_lock=False))
add("checkout_lock_held", install_root="/install", io=clh_io(_try_shared_lock=script(raise_spec=OSERR)))
add("checkout_lock_held", install_root="/install", io=clh_io(**{"os.close": script(raise_spec=RT)}))
add("checkout_lock_held", install_root="/install", io=clh_io(_try_shared_lock=script(raise_spec=KB)))

# ── _marker_unwritable ──────────────────────────────────────────────────────
add("_marker_unwritable", path=PATH, exc={"type": "PermissionError", "message": "denied", "errno": 13}, io={})
add("_marker_unwritable", path=PATH, exc={"type": "OSError", "message": "read-only"}, io={})
add("_marker_unwritable", path="/a/b", exc={"type": "OSError", "message": "no space", "errno": 28}, io={})
add("_marker_unwritable", path=PATH, exc={"type": "OSError", "message": "x", "errno": 5}, io={})
add("_marker_unwritable", path="/x/.m", exc={"type": "PermissionError", "message": "denied", "errno": 13}, io={})

# ── _release_marker ─────────────────────────────────────────────────────────
def rm_io(**over):
    io = {"mutex.enter": script(None), "mutex.exit": script(None),
          "_read_bytes": "200\n1999\nct:1999\n",
          "_release_decision": {"action": "delete", "new": None},
          "path.unlink": script(None), "_compare_and_swap": True}
    io.update(over)
    return io


add("_release_marker", path=PATH, io=rm_io(_read_bytes=None))
add("_release_marker", path=PATH, io=rm_io())
add("_release_marker", path=PATH,
    io=rm_io(_release_decision={"action": "rewrite", "new": "100\n1999\nct:1000\n"}))
add("_release_marker", path=PATH, io=rm_io(_release_decision={"action": "keep", "new": None}))
add("_release_marker", path=PATH, io=rm_io(**{"path.unlink": script(raise_spec=FNF)}))
add("_release_marker", path=PATH, io=rm_io(_read_bytes=script(raise_spec=RT)))
add("_release_marker", path=PATH, io=rm_io(**{"mutex.enter": script(raise_spec={"type": "MarkerBusy", "message": "held"})}))
add("_release_marker", path=PATH,
    io=rm_io(_release_decision={"action": "rewrite", "new": "n"}, _compare_and_swap=script(raise_spec=RT)))

# ── acquire_checkout (UpdateLock) ───────────────────────────────────────────
add("acquire_checkout", self=selfspec(_checkout=True), install_root="/install", io={})
add("acquire_checkout", self=selfspec(), install_root="/install", io={"_acquire_checkout": None})
add("acquire_checkout", self=selfspec(), install_root="/install",
    io={"_acquire_checkout": {"pid": 200, "age_seconds": 5.0}})
add("acquire_checkout", self=selfspec(), install_root="/install",
    io={"_acquire_checkout": {"pid": 0, "age_seconds": 0.0, "reason": "not writable"}})
add("acquire_checkout", self=selfspec(), install_root="/install",
    io={"_acquire_checkout": script(raise_spec=RT)})
add("acquire_checkout", self=selfspec(_checkout=True), install_root=None, io={})

# ── _checkout_held_elsewhere (UpdateLock) ───────────────────────────────────
add("_checkout_held_elsewhere", self=selfspec(_checkout=True), held=None, io={})
add("_checkout_held_elsewhere", self=selfspec(), held=heldspec(path="/x"),
    io={"checkout_lock_path": "/x"})
add("_checkout_held_elsewhere", self=selfspec(), held=None,
    io={"checkout_lock_path": "/x", "checkout_lock_held": False})
add("_checkout_held_elsewhere", self=selfspec(), held=None,
    io={"checkout_lock_path": "/x", "checkout_lock_held": True, "_inherited_lock_fd": None})
add("_checkout_held_elsewhere", self=selfspec(), held=None,
    io={"checkout_lock_path": "/x", "checkout_lock_held": True, "_inherited_lock_fd": 5})
add("_checkout_held_elsewhere", self=selfspec(), held=None,
    io={"checkout_lock_path": script(raise_spec=RT)})
add("_checkout_held_elsewhere", self=selfspec(install_root="/install"), held=heldspec(path="/other"),
    io={"checkout_lock_path": "/x", "checkout_lock_held": True, "_inherited_lock_fd": None})

# ── _is_partner (UpdateLock staticmethod) ───────────────────────────────────
add("_is_partner", pid=100, io={"os.getpid": 100})
add("_is_partner", pid=0, io={"os.getpid": 100})
add("_is_partner", pid=200, io={"os.getpid": 100, "_handoff_pid": 200})
add("_is_partner", pid=200, io={"os.getpid": 100, "_handoff_pid": None, "_is_ancestor_pid": True})
add("_is_partner", pid=200, io={"os.getpid": 100, "_handoff_pid": 300, "_is_ancestor_pid": False})
add("_is_partner", pid=200, io={"os.getpid": script(raise_spec=RT)})
add("_is_partner", pid=200, io={"os.getpid": 100, "_handoff_pid": script(raise_spec=KB)})

# ── _adopt_or_refuse (UpdateLock) ───────────────────────────────────────────
def adopt_io(**over):
    io = {"_live_partners": script([200]), "self._is_partner": False,
          "os.getpid": 100, "_runtime_host_below": False,
          "_identity_line": "ct:1000.000", "marker.age": 5.0,
          "_compare_and_swap": True}
    io.update(over)
    return io


add("_adopt_or_refuse", self=selfspec(), marker=marker(), io=adopt_io())
add("_adopt_or_refuse", self=selfspec(), marker=marker(),
    io=adopt_io(**{"self._is_partner": True, "os.getpid": 200}))
add("_adopt_or_refuse", self=selfspec(), marker=marker(),
    io=adopt_io(**{"self._is_partner": True, "os.getpid": 100, "_runtime_host_below": False}))
add("_adopt_or_refuse", self=selfspec(), marker=marker(),
    io=adopt_io(**{"self._is_partner": True, "os.getpid": 100, "_runtime_host_below": True}))
add("_adopt_or_refuse", self=selfspec(),
    marker=marker(runs=["r1"], delegate_pid=300, delegate_create_time=1999.0, delegate_ct_text="1999",
                  raw="200\n1999\nct:1999\ndelegate:300 ct:1999\nrun:r1\n"),
    io=adopt_io(**{"self._is_partner": True, "os.getpid": 100, "_runtime_host_below": False}))
add("_adopt_or_refuse", self=selfspec(), marker=marker(),
    io=adopt_io(_live_partners=script(raise_spec=RT)))
add("_adopt_or_refuse", self=selfspec(), marker=marker(create_time=None, ct_text=None, raw="200\n1999\n"),
    io=adopt_io(**{"self._is_partner": True, "os.getpid": 100, "_runtime_host_below": False}))
add("_adopt_or_refuse", self=selfspec(), marker=marker(started_at=None, create_time=None, ct_text=None, raw="200\n"),
    io=adopt_io(**{"self._is_partner": False, "marker.age": 0.0}))

# ── _drop_checkout (UpdateLock) ─────────────────────────────────────────────
add("_drop_checkout", self=selfspec(_checkout=True), io={"_release_checkout": script(None)})
add("_drop_checkout", self=selfspec(_checkout=False), io={})
add("_drop_checkout", self=selfspec(_checkout=True), io={"_release_checkout": script(raise_spec=RT)})
add("_drop_checkout", self=selfspec(_checkout=True, acquired=True), io={"_release_checkout": script(None)})
add("_drop_checkout", self=selfspec(_checkout=False, acquired=True), io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for f, n in sorted(counts.items()):
        print(f"  {f}: {n}")
    assert len(counts) == 30, f"expected 30 funcs, got {len(counts)}"
    assert all(n >= 5 for n in counts.values()), "every port needs >=5 cases"


if __name__ == "__main__":
    main()
