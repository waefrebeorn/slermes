#!/usr/bin/env python3
"""Generate tests/fixtures/local_env_flow.jsonl for the local_env_flow oracle.

Each case is {"func": <name>, "in": {...}} where "in" carries the call arguments,
the state objects (env / result / proc / descendants), platform / import flags,
and "io": the per-seam scripts. Script semantics (identical in the C harness and
the oracle): a single value is reused for every call; a bare array is consumed
one element per call (last repeats when exhausted); {"__script__":true,"ret":X}
is a constant (list) result. A {"__raise__":Kind,"msg":...} value scripts an
exception. >=5 cases per ported function.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "local_env_flow.jsonl")


def script_list(x):
    return {"__script__": True, "ret": x}


def raise_(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── 1. prepend_missing_path_entries (pure leaf) ──
add("prepend_missing_path_entries", existing_path="/usr/bin:/bin", dirs=["/opt/x", "/usr/bin"], pathsep=":")
add("prepend_missing_path_entries", existing_path="/usr/bin:/bin", dirs=["/bin", "/usr/bin"], pathsep=":")
add("prepend_missing_path_entries", existing_path="", dirs=["/a"], pathsep=":")
add("prepend_missing_path_entries", existing_path="/a::/b:", dirs=["/c"], pathsep=":")
add("prepend_missing_path_entries", existing_path="/a:/b", dirs=[], pathsep=":")
add("prepend_missing_path_entries", existing_path="C:\\x;C:\\y", dirs=["C:\\z", "C:\\x"], pathsep=";")
add("prepend_missing_path_entries", existing_path="/a", dirs=["/b", "/b"], pathsep=":")

# ── 2. append_dropped_path_entries ──
add("append_dropped_path_entries", cmd_string="echo hi", parent_path="/a:/b", is_windows=True)
add("append_dropped_path_entries", cmd_string="echo hi", parent_path="/a:/b", is_windows=False,
    io={"isabs": [True, True], "isdir": [True, False]})
add("append_dropped_path_entries", cmd_string="echo hi", parent_path="rel:/a", is_windows=False,
    io={"isabs": [False, True], "isdir": [True]})
add("append_dropped_path_entries", cmd_string="echo hi", parent_path="", is_windows=False)
add("append_dropped_path_entries", cmd_string="echo hi", parent_path="/a:/a:/b", is_windows=False,
    io={"isabs": [True, True], "isdir": [True, True]})
add("append_dropped_path_entries", cmd_string="echo hi", parent_path="/a", is_windows=False,
    io={"isabs": raise_("RuntimeError", "boom")})
add("append_dropped_path_entries", cmd_string="echo hi", parent_path="/a:/b", is_windows=False,
    io={"isabs": [True, False], "isdir": [True]})

# ── 3. compute_git_bash_bin_dirs ──
add("compute_git_bash_bin_dirs", io={
    "find_bash": "/git/bin/bash",
    "dirname": ["/git/bin", "/git"],
    "basename": ["git"],
    "path_join": ["/git/ucrt64/bin", "/git/clangarm64/bin", "/git/mingw64/bin", "/git/mingw32/bin",
                  "/git/usr/local/bin", "/git/usr/bin", "/git/bin"],
    "isdir": [True, False, True, False, False, True, True]})
add("compute_git_bash_bin_dirs", io={
    "find_bash": "/git/usr/bin/bash",
    "dirname": ["/git/usr/bin", "/git/usr", "/git"],
    "basename": ["usr"],
    "path_join": ["/git/ucrt64/bin", "/git/clangarm64/bin", "/git/mingw64/bin", "/git/mingw32/bin",
                  "/git/usr/local/bin", "/git/usr/bin", "/git/bin"],
    "isdir": [False, False, False, False, False, False, True]})
add("compute_git_bash_bin_dirs", io={"find_bash": raise_("RuntimeError", "no shell")})
add("compute_git_bash_bin_dirs", io={"find_bash": raise_("KeyboardInterrupt", "ki")})
add("compute_git_bash_bin_dirs", io={
    "find_bash": "/g/bin/bash",
    "dirname": ["/g/bin", "/g"],
    "basename": ["bin"],
    "path_join": ["/g/bin", "/g/bin", "/g/x", "/g/x", "/g/y", "/g/y", "/g/z"],
    "isdir": [True, True, True, True, True, True, True]})
add("compute_git_bash_bin_dirs", io={
    "find_bash": "/g/bin/bash",
    "dirname": ["/g/bin", "/g"],
    "basename": ["bin"],
    "path_join": ["/g/a", "/g/b"],
    "isdir": [True, raise_("OSError", "x")]})

# ── 4. user_tools_first ──
add("user_tools_first", existing_path="/usr/bin:/bin", pathsep=":", io={
    "user_facing_path": "/usr/bin:/bin",
    "git_bash_bin_dirs": script_list(["/git/bin"]),
    "normcase": ["/git/bin", "/usr/bin", "/bin"]})
add("user_tools_first", existing_path="/usr/bin:/bin", pathsep=":", io={
    "user_facing_path": "/usr/bin:/bin",
    "git_bash_bin_dirs": script_list([]),
    "normcase": ["/usr/bin", "/bin"]})
add("user_tools_first", existing_path="/git/bin:/usr/bin", pathsep=":", io={
    "user_facing_path": "/git/bin:/usr/bin",
    "git_bash_bin_dirs": script_list(["/Git/Bin"]),
    "normcase": ["/git/bin", "/git/bin", "/usr/bin"]})
add("user_tools_first", existing_path="/a::/b", pathsep=":", io={
    "user_facing_path": "/a::/b",
    "git_bash_bin_dirs": script_list(["/g"]),
    "normcase": ["/g", "/a", "/b"]})
add("user_tools_first", existing_path="/x", pathsep=":", io={
    "user_facing_path": raise_("RuntimeError", "uf")})
add("user_tools_first", existing_path="/x", pathsep=":", io={
    "user_facing_path": "/x",
    "git_bash_bin_dirs": raise_("OSError", "gb")})

# ── 5. terminal_env ──
add("terminal_env", env={"PATH": "/usr/bin"}, pathsep=":", io={
    "path_env_key": "PATH",
    "user_facing_path": "/usr/bin",
    "git_bash_bin_dirs": script_list(["/g"]),
    "normcase": ["/g", "/usr/bin"]})
add("terminal_env", env={"PATH": "/x"}, pathsep=":", io={"path_env_key": None})
add("terminal_env", env={"Path": "/x"}, pathsep=":", io={
    "path_env_key": "Path",
    "user_facing_path": "/x",
    "git_bash_bin_dirs": script_list([]),
    "normcase": ["/x"]})
add("terminal_env", env={"FOO": "bar"}, pathsep=":", io={"path_env_key": "PATH"})
add("terminal_env", env={"PATH": "/x"}, pathsep=":", io={"path_env_key": raise_("RuntimeError", "pk")})
add("terminal_env", env={"PATH": "/x"}, pathsep=":", io={
    "path_env_key": "PATH",
    "user_facing_path": raise_("OSError", "uf")})

# ── 6. apply_profile_home ──
add("apply_profile_home", env={}, io={
    "get_hermes_home_override": "/home/x",
    "apply_subprocess_home_env": {"HOME": "/home/x"}})
add("apply_profile_home", env={}, io={
    "get_hermes_home_override": "",
    "apply_subprocess_home_env": {"HOME": "/default"}})
add("apply_profile_home", env={}, io={
    "get_hermes_home_override": None,
    "apply_subprocess_home_env": None})
add("apply_profile_home", env={}, io={
    "get_hermes_home_override": raise_("RuntimeError", "ov"),
    "apply_subprocess_home_env": {"HOME": "/h"}})
add("apply_profile_home", env={}, io={
    "get_hermes_home_override": raise_("KeyboardInterrupt", "ki")})
add("apply_profile_home", env={}, io={
    "get_hermes_home_override": "/home/x",
    "apply_subprocess_home_env": raise_("OSError", "ap")})

# ── 7. carry_bridge_markers ──
add("carry_bridge_markers", env={"HERMES_BRIDGED_X": "d1", "X": "v", "OTHER": "o"},
    source={}, own_profile=False)
add("carry_bridge_markers", env={"X": "v"}, source={"HERMES_BRIDGED_X": "sha256:abc"},
    own_profile=True, io={"bridged_env_digest": "sha256:abc"})
add("carry_bridge_markers", env={"X": "v"}, source={"HERMES_BRIDGED_X": "sha256:abc"},
    own_profile=True, io={"bridged_env_digest": "sha256:OTHER"})
add("carry_bridge_markers", env={"hermes_bridged_y": "d", "Z": "w"},
    source={"HERMES_BRIDGED_X": "sha256:n"}, own_profile=True, io={"bridged_env_digest": None})
add("carry_bridge_markers", env={}, source={"HERMES_BRIDGED_X": None},
    own_profile=True, io={"bridged_env_digest": None})
add("carry_bridge_markers", env={"A": "1"}, source={"X": "v", "HERMES_BRIDGED_": "d"},
    own_profile=True)
add("carry_bridge_markers", env={"X": "v"}, source={"HERMES_BRIDGED_X": "sha256:abc"},
    own_profile=True, io={"bridged_env_digest": raise_("RuntimeError", "dg")})

# ── 8. restore_bootstrap_tokens ──
add("restore_bootstrap_tokens", env={}, source={"TOK_A": "a", "TOK_B": ""},
    io={"secret_source_bootstrap_env": script_list(["TOK_A", "TOK_B"])})
add("restore_bootstrap_tokens", env={"TOK_A": "existing"}, source={"TOK_A": "new"},
    io={"secret_source_bootstrap_env": script_list(["TOK_A"])})
add("restore_bootstrap_tokens", env={}, source={"X": "1"},
    io={"secret_source_bootstrap_env": script_list([])})
add("restore_bootstrap_tokens", env={"A": "0"}, source={"A": "a", "B": "b"},
    io={"secret_source_bootstrap_env": script_list(["A", "B"])})
add("restore_bootstrap_tokens", env={}, source={},
    io={"secret_source_bootstrap_env": raise_("RuntimeError", "bs")})
add("restore_bootstrap_tokens", env={}, source={},
    io={"secret_source_bootstrap_env": script_list(["A"])})

# ── 9. usable_cwd ──
add("usable_cwd", cwd="/abs/path", self_cwd="/base", io={
    "expanduser": "/abs/path", "msys_to_windows_path": "/abs/path",
    "isabs": True, "resolve_safe_cwd": "/abs/path"})
add("usable_cwd", cwd="rel", self_cwd="/base", io={
    "expanduser": "rel", "msys_to_windows_path": "rel",
    "isabs": False, "path_join": "/base/rel", "resolve_safe_cwd": "/base/rel"})
add("usable_cwd", cwd="/gone", self_cwd="/base", io={
    "expanduser": "/gone", "msys_to_windows_path": ["/gone", "/gone"],
    "isabs": True, "resolve_safe_cwd": "/fallback"})
add("usable_cwd", cwd="/c/x", self_cwd="/base", io={
    "expanduser": "/c/x", "msys_to_windows_path": ["C:\\x", "C:\\x"],
    "isabs": True, "resolve_safe_cwd": "C:\\x"})
add("usable_cwd", cwd="", self_cwd="/base", io={
    "expanduser": "", "msys_to_windows_path": "", "resolve_safe_cwd": "/tmp"})
add("usable_cwd", cwd="/x", self_cwd="/base", io={"expanduser": raise_("RuntimeError", "eu")})
add("usable_cwd", cwd="/x", self_cwd="/base", io={
    "expanduser": "/x", "msys_to_windows_path": "/x", "isabs": True,
    "resolve_safe_cwd": raise_("OSError", "rs")})

# ── 10. parse_cwd_marker ──
add("parse_cwd_marker", result={"output": "raw"}, io={
    "base_parse_cwd_marker": {"cwd_observed": True, "cwd": "/c/x", "output": "clean"},
    "msys_to_windows_path": "C:\\x", "isdir": True})
add("parse_cwd_marker", result={"output": "raw"}, io={
    "base_parse_cwd_marker": {"cwd_observed": True, "cwd": "/gone", "output": "clean"},
    "msys_to_windows_path": "/gone", "isdir": False})
add("parse_cwd_marker", result={"output": "x"}, io={"base_parse_cwd_marker": None})
add("parse_cwd_marker", result={"output": "x"}, io={
    "base_parse_cwd_marker": {"cwd_observed": True}})
add("parse_cwd_marker", result={"output": "x"}, io={
    "base_parse_cwd_marker": {"cwd_observed": True, "cwd": "", "output": "x"},
    "msys_to_windows_path": ""})
add("parse_cwd_marker", result={"output": "x"}, io={
    "base_parse_cwd_marker": raise_("RuntimeError", "bp")})
add("parse_cwd_marker", result={"output": "x"}, io={
    "base_parse_cwd_marker": {"cwd_observed": True, "cwd": "/c/x"},
    "msys_to_windows_path": "C:\\x", "isdir": raise_("OSError", "id")})

# ── 11. leader_is_ours ──
add("leader_is_ours", pgid=None, expected_start=None)
add("leader_is_ours", pgid=100, expected_start=None)
add("leader_is_ours", pgid=100, expected_start="fp1", io={
    "get_process_start_time": "fp2", "fingerprints_match": True})
add("leader_is_ours", pgid=100, expected_start="fp1", io={
    "get_process_start_time": "fp2", "fingerprints_match": False})
add("leader_is_ours", pgid=100, expected_start="fp1", io={
    "get_process_start_time": raise_("RuntimeError", "gp")})
add("leader_is_ours", pgid=100, expected_start="fp1", io={"get_process_start_time": None})
add("leader_is_ours", pgid=100, expected_start="fp1", io={
    "get_process_start_time": "fp2", "fingerprints_match": raise_("ValueError", "fp")})
add("leader_is_ours", pgid=100, expected_start="fp1", io={
    "get_process_start_time": "fp2", "fingerprints_match": raise_("RuntimeError", "fp")})
add("leader_is_ours", pgid=100, expected_start="fp1", io={
    "get_process_start_time": raise_("KeyboardInterrupt", "ki")})

# ── 12. wait_for_group_exit ──
add("wait_for_group_exit", proc={"pid": 50}, pgid=100, timeout=1.0, io={
    "monotonic": [0.0], "proc_poll": None,
    "killpg": [raise_("ProcessLookupError", "gone")]})
add("wait_for_group_exit", proc={"pid": 50}, pgid=100, timeout=1.0, io={
    "monotonic": [0.0, 0.6, 1.1], "proc_poll": None, "killpg": [None, None], "sleep": None})
add("wait_for_group_exit", proc={"pid": 50}, pgid=100, timeout=1.0, io={
    "monotonic": [0.0, 1.1], "proc_poll": None,
    "killpg": [raise_("PermissionError", "perm")]})
add("wait_for_group_exit", proc={"pid": 50}, pgid=100, timeout=1.0, io={
    "monotonic": [0.0], "proc_poll": raise_("RuntimeError", "poll"),
    "killpg": [raise_("ProcessLookupError", "gone")]})
add("wait_for_group_exit", proc={"pid": 50}, pgid=100, timeout=1.0, io={
    "monotonic": [0.0], "proc_poll": None, "killpg": [raise_("OSError", "x")]})
add("wait_for_group_exit", proc={"pid": 50}, pgid=100, timeout=1.0, io={
    "monotonic": [0.0], "proc_poll": raise_("KeyboardInterrupt", "ki")})
add("wait_for_group_exit", proc={"pid": 50}, pgid=100, timeout=1.0, io={
    "monotonic": [0.0, 0.5], "proc_poll": None, "killpg": [None],
    "sleep": raise_("RuntimeError", "sleep")})

# ── 13. sweep_escaped_descendants ──
add("sweep_escaped_descendants", descendants=[{"pid": 101}], pgid=100, io={
    "child_is_running": True, "getpgid": 999, "child_kill": None})
add("sweep_escaped_descendants", descendants=[{"pid": 101}], pgid=100, io={
    "child_is_running": False})
add("sweep_escaped_descendants", descendants=[{"pid": 101}], pgid=100, io={
    "child_is_running": True, "getpgid": 100})
add("sweep_escaped_descendants", descendants=[{"pid": 101}], pgid=100, io={
    "child_is_running": True, "getpgid": raise_("ProcessLookupError", "x"), "child_kill": None})
add("sweep_escaped_descendants", descendants=[{"pid": 101}], pgid=100, io={
    "child_is_running": raise_("RuntimeError", "x")})
add("sweep_escaped_descendants", descendants=[{"pid": 101}, {"pid": 102}], pgid=100, io={
    "child_is_running": [True, True], "getpgid": [999, 998],
    "child_kill": [raise_("OSError", "x"), None]})
add("sweep_escaped_descendants", descendants=[{"pid": 101}], pgid=100, io={
    "child_is_running": True, "getpgid": raise_("RuntimeError", "x")})
add("sweep_escaped_descendants", descendants=[{"pid": 101}], pgid=100, io={
    "child_is_running": raise_("KeyboardInterrupt", "ki")})

# ── 14. kill_known_pids ──
add("kill_known_pids", proc={"pid": 50}, descendants=[{"pid": 101}, {"pid": 102}], io={
    "proc_kill": None, "child_kill": [None, None]})
add("kill_known_pids", proc={"pid": 50}, descendants=[{"pid": 101}], io={
    "proc_kill": raise_("OSError", "x"), "child_kill": [None]})
add("kill_known_pids", proc={"pid": 50}, descendants=[{"pid": 101}], io={
    "proc_kill": raise_("KeyboardInterrupt", "ki")})
add("kill_known_pids", proc={"pid": 50}, descendants=[], io={"proc_kill": None})
add("kill_known_pids", proc={"pid": 50}, descendants=[{"pid": 101}, {"pid": 102}], io={
    "proc_kill": None, "child_kill": [raise_("RuntimeError", "x"), None]})

# ── 15. kill_process_group_posix ──
add("kill_process_group_posix", proc={"pid": 50, "_hermes_pgid_start": "fp_old"}, io={
    "getpgid": [100, 999], "get_process_start_time": "fp_new", "fingerprints_match": False,
    "psutil_children": script_list([{"pid": 101}]),
    "child_is_running": True, "child_kill": None})
add("kill_process_group_posix", proc={"pid": 50, "_hermes_pgid_start": None}, io={
    "getpgid": [100, 100], "psutil_children": script_list([{"pid": 101}]),
    "getpgrp": 100, "proc_kill": None, "child_kill": None, "child_is_running": True})
add("kill_process_group_posix", proc={"pid": 50, "_hermes_pgid_start": None}, io={
    "getpgid": [100, 999], "psutil_children": script_list([{"pid": 101}]),
    "getpgrp": 1, "killpg": [None, raise_("ProcessLookupError", "gone")],
    "monotonic": [0.0], "proc_poll": None, "child_is_running": True, "child_kill": None})
add("kill_process_group_posix", proc={"pid": 50, "_hermes_pgid_start": None}, io={
    "getpgid": [100, 999], "psutil_children": script_list([{"pid": 101}]),
    "getpgrp": 1,
    "killpg": [None, None, None, None, raise_("ProcessLookupError", "gone")],
    "monotonic": [0.0, 0.6, 1.1, 100], "proc_poll": None, "sleep": None, "proc_wait": None,
    "child_is_running": True, "child_kill": None})
add("kill_process_group_posix", proc={"pid": 50, "_hermes_pgid": 200, "_hermes_pgid_start": None}, io={
    "getpgid": raise_("ProcessLookupError", "x"), "psutil_children": script_list([]),
    "getpgrp": 1, "killpg": [None, raise_("ProcessLookupError", "gone")],
    "monotonic": [0.0], "proc_poll": None})
add("kill_process_group_posix", proc={"pid": 50}, io={
    "getpgid": raise_("ProcessLookupError", "no proc")})
add("kill_process_group_posix", proc={"pid": 50, "_hermes_pgid_start": None},
    psutil_import_fail=True, io={
        "getpgid": 100, "getpgrp": 1,
        "killpg": [None, raise_("ProcessLookupError", "gone")],
        "monotonic": [0.0], "proc_poll": None})
add("kill_process_group_posix", proc={"pid": 50, "_hermes_pgid_start": None}, io={
    "getpgid": [100, 999], "psutil_children": script_list([{"pid": 101}]),
    "getpgrp": 1, "killpg": raise_("PermissionError", "perm"),
    "proc_kill": None, "child_kill": [None, None], "child_is_running": True})

# ── 16. kill_process_windows ──
add("kill_process_windows", proc={"pid": 50}, io={
    "get_process_start_time": "fp", "terminate_pid": None, "proc_wait": None})
add("kill_process_windows", proc={"pid": 50}, io={
    "get_process_start_time": raise_("RuntimeError", "gp"),
    "proc_kill": None, "proc_wait": None})
add("kill_process_windows", proc={"pid": 50}, io={
    "get_process_start_time": "fp", "terminate_pid": raise_("OSError", "tp"),
    "proc_kill": None, "proc_wait": None})
add("kill_process_windows", proc={"pid": 50}, status_import_fail=True, io={
    "proc_kill": None, "proc_wait": None})
add("kill_process_windows", proc={"pid": 50}, io={
    "get_process_start_time": "fp", "terminate_pid": None,
    "proc_wait": raise_("TimeoutExpired", "Command 'proc' timed out after 2.0 seconds")})
add("kill_process_windows", proc={"pid": 50}, io={
    "get_process_start_time": "fp", "terminate_pid": None,
    "proc_wait": raise_("RuntimeError", "pw")})
add("kill_process_windows", proc={"pid": 50}, io={
    "get_process_start_time": raise_("KeyboardInterrupt", "ki")})
add("kill_process_windows", proc={"pid": 50}, io={
    "get_process_start_time": None, "terminate_pid": None, "proc_wait": None})

# ── 17. force_kill_process ──
add("force_kill_process", proc={"pid": 50}, is_windows=True, io={"kill_process": None})
add("force_kill_process", proc={"pid": 50, "_hermes_pgid": 100, "_hermes_pgid_start": None},
    is_windows=False, io={"getpgrp": 1, "killpg": None, "proc_kill": None})
add("force_kill_process", proc={"pid": 50, "_hermes_pgid": None, "_hermes_pgid_start": None},
    is_windows=False, io={"getpgid": 100, "getpgrp": 100, "proc_kill": None})
add("force_kill_process", proc={"pid": 50, "_hermes_pgid": 0, "_hermes_pgid_start": "fp1"},
    is_windows=False, io={
        "getpgid": 100, "getpgrp": 1, "get_process_start_time": "fp2",
        "fingerprints_match": False, "proc_kill": None})
add("force_kill_process", proc={"pid": 50, "_hermes_pgid": None}, is_windows=False, io={
    "getpgid": raise_("ProcessLookupError", "x"), "proc_kill": None})
add("force_kill_process", proc={"pid": 50, "_hermes_pgid": 100, "_hermes_pgid_start": None},
    is_windows=False, io={"getpgrp": 1, "killpg": raise_("OSError", "x"), "proc_kill": None})
add("force_kill_process", proc={"pid": 50, "_hermes_pgid": 100, "_hermes_pgid_start": None},
    is_windows=False, io={"getpgrp": 100, "proc_kill": raise_("OSError", "x")})
add("force_kill_process", proc={"pid": 50, "_hermes_pgid": 100}, is_windows=False, io={
    "getpgrp": raise_("RuntimeError", "x")})
add("force_kill_process", proc={"pid": 50}, is_windows=True, io={
    "kill_process": raise_("RuntimeError", "x")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-function counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %s: %d cases, %d functions" % (OUT, len(CASES), len(counts)))
    for fn in sorted(counts):
        flag = "  (<5!)" if counts[fn] < 5 else ""
        print("  %-32s %d%s" % (fn, counts[fn], flag))
    assert all(v >= 5 for v in counts.values()), "a function has <5 cases"


if __name__ == "__main__":
    main()