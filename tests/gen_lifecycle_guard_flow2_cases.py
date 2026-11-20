#!/usr/bin/env python3
"""Generator for tests/fixtures/lifecycle_guard_flow2.jsonl.

Emits >=5 cases per UNCLAIMED function of cron/lifecycle_guard.py ported as
src/cron/lifecycle_guard_flow2.c. Each case is
{"func": <name>, "in": {...args..., "io": {seam: script}, "env": {...}}}.

Script shapes (shared by lg2_call and the oracle):
  absent key             -> None
  {"$seq": [...]}        -> per-call queue (exhaustion raises RuntimeError)
  {"__raise__": cls, "msg": m} -> raises a real exception (trace ret is marker)
  any other JSON value   -> reused for every call

Domain: ASCII strings; read data encoded latin-1; Path spelling is a verbatim
string. Integral floats collapse to ints on both sides.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "lifecycle_guard_flow2.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*vals):
    return {"$seq": list(vals)}


def exc(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


B = lambda b: bool(b)

# ── _is_interpreter_image ──────────────────────────────────────────────────
case("_is_interpreter_image", value="python3.12")
case("_is_interpreter_image", value="pythonw3.exe")
case("_is_interpreter_image", value="chrome.exe")
case("_is_interpreter_image", value="python*")
case("_is_interpreter_image", value="  \"PYTHON\"  ")
case("_is_interpreter_image", value="py", substring=True)
case("_is_interpreter_image", value="   ", substring=True)

# ── _pattern_reaches_host_interpreter ──────────────────────────────────────
case("_pattern_reaches_host_interpreter", pattern="python3.12", full_cmdline=False, exact=True)
case("_pattern_reaches_host_interpreter", pattern="python3?", full_cmdline=False, exact=False)
case("_pattern_reaches_host_interpreter", pattern="python.*", full_cmdline=False, exact=False)
case("_pattern_reaches_host_interpreter", pattern="python -m hermes_cli.main gateway run", full_cmdline=True, exact=False)
case("_pattern_reaches_host_interpreter", pattern="python mt_add.py", full_cmdline=True, exact=False)
case("_pattern_reaches_host_interpreter", pattern="chrome", full_cmdline=True, exact=True)

# ── _killer_targets_host_interpreter ───────────────────────────────────────
case("_killer_targets_host_interpreter", name="pkill", args=["-9", "python3"])
case("_killer_targets_host_interpreter", name="pkill", args=["-u", "root", "chrome"])
case("_killer_targets_host_interpreter", name="pidof", args=["python"])
case("_killer_targets_host_interpreter", name="taskkill", args=["/F", "/IM", "python.exe"])
case("_killer_targets_host_interpreter", name="taskkill", args=["/FI", "imagename eq python.exe"])
case("_killer_targets_host_interpreter", name="Stop-Process", args=["-Name", "python,chrome"])

# ── _segment_names_host_interpreter ────────────────────────────────────────
_SNAME = io(command_token_index=0, peel_transparent_prefixes=0,
            executable_name=seq("pkill", "pkill"))
case("_segment_names_host_interpreter", tokens=["pkill", "-9", "python3"], killers=True, io=_SNAME)
case("_segment_names_host_interpreter", tokens=["sudo", "pkill", "python"], killers=True,
     io=io(command_token_index=0, peel_transparent_prefixes=1, executable_name=seq("sudo", "pkill", "pkill")))
case("_segment_names_host_interpreter", tokens=["echo", "hi"], killers=True,
     io=io(command_token_index=0, peel_transparent_prefixes=0, executable_name=seq("echo", "echo")))
case("_segment_names_host_interpreter", tokens=["pgrep", "python"], killers=False,
     io=io(command_token_index=0, peel_transparent_prefixes=0, executable_name=seq("pgrep", "pgrep")))
case("_segment_names_host_interpreter", tokens=[], killers=True,
     io=io(command_token_index=None, peel_transparent_prefixes=None, executable_name="x"))

# ── contains_host_interpreter_kill ─────────────────────────────────────────
_CSEG = io(iter_command_segments=[["pkill", "-9", "python3"]],
           command_token_index=0, peel_transparent_prefixes=0,
           executable_name=seq("pkill", "pkill", "pkill"))
case("contains_host_interpreter_kill", text="pkill -9 python3", io=_CSEG)
case("contains_host_interpreter_kill", text="echo hello",
     io=io(iter_command_segments=[["echo", "hello"]], command_token_index=0,
           peel_transparent_prefixes=0, executable_name=seq("echo", "echo", "echo")))
case("contains_host_interpreter_kill", text="pkill -9 python3",
     io=io(iter_command_segments=[], command_token_index=0, peel_transparent_prefixes=0, executable_name="pkill"))
case("contains_host_interpreter_kill", text="kill 1234",
     io=io(iter_command_segments=[["kill", "1234"]], command_token_index=0,
           peel_transparent_prefixes=0, executable_name=seq("kill", "kill", "kill")))
case("contains_host_interpreter_kill", text="", io=io(iter_command_segments=[]))

# ── first_gateway_lifecycle_match ──────────────────────────────────────────
case("first_gateway_lifecycle_match", text="hermes gateway restart",
     io=io(strip_inert_heredoc_bodies="hermes gateway restart", iter_command_segments=[]))
case("first_gateway_lifecycle_match", text="echo hi",
     io=io(strip_inert_heredoc_bodies="echo hi", iter_command_segments=[]))
case("first_gateway_lifecycle_match", text="launchctl kickstart ai.hermes.gateway",
     io=io(strip_inert_heredoc_bodies="launchctl kickstart ai.hermes.gateway", iter_command_segments=[]))
case("first_gateway_lifecycle_match", text="pkill -f hermes-gateway",
     io=io(strip_inert_heredoc_bodies="pkill -f hermes-gateway", iter_command_segments=[]))
case("first_gateway_lifecycle_match", text="", io=io(strip_inert_heredoc_bodies="", iter_command_segments=[]))
case("first_gateway_lifecycle_match", text="x; hermes gateway stop # c",
     io=io(strip_inert_heredoc_bodies="x; hermes gateway stop # c", iter_command_segments=[]))

# ── _capped_read_limit ─────────────────────────────────────────────────────
case("_capped_read_limit", max_bytes=None)
case("_capped_read_limit", max_bytes=100)
case("_capped_read_limit", max_bytes=2000000)
case("_capped_read_limit", max_bytes=-5)
case("_capped_read_limit", max_bytes=0)
case("_capped_read_limit", max_bytes=1048576)

# ── lifecycle_scan_root_within_budget ──────────────────────────────────────
case("lifecycle_scan_root_within_budget", text="echo hello")
case("lifecycle_scan_root_within_budget", text="a\nb\nc")
case("lifecycle_scan_root_within_budget", text="é" * 5)
case("lifecycle_scan_root_within_budget", text="", env={"max_scan_bytes": 0})
case("lifecycle_scan_root_within_budget", text="abcd", env={"max_scan_bytes": 2})
case("lifecycle_scan_root_within_budget", text="a\nb", env={"max_scan_lines": 1})

# ── budget class methods ───────────────────────────────────────────────────
_BUDGET = {"bytes_remaining": 64, "lines_remaining": 8, "paths_remaining": 3,
           "remote_reads_remaining": 2, "refusal": None, "scanned_bodies": {}}
case("__init__", env={"max_scan_bytes": 64, "max_scan_lines": 8})
case("__init__", env={"max_scan_paths": 2})
case("__init__", env={"max_remote_reads": 3})
case("__init__", env={"max_scan_bytes": 10, "max_scan_lines": 4, "max_scan_paths": 1})
case("__init__", env={})
case("__init__", env={"max_line_bytes": 12})
case("charge_text", budget=_BUDGET, text="hello world", written_body=False)
case("charge_text", budget=_BUDGET, text="body", written_body=True)
case("charge_text", budget={"bytes_remaining": 2, "lines_remaining": 8, "paths_remaining": 3,
      "remote_reads_remaining": 2, "refusal": None, "scanned_bodies": {}}, text="hello")
case("charge_text", budget=_BUDGET, text="a\nb\nc", written_body=False)
case("charge_text", budget=_BUDGET, text="body", written_body=True)
case("charge_text", budget=_BUDGET, text="", written_body=False)
case("charge_path", budget=_BUDGET)
case("charge_path", budget={**_BUDGET, "paths_remaining": 0})
case("charge_path", budget={**_BUDGET, "paths_remaining": 1})
case("charge_path", budget={**_BUDGET, "paths_remaining": 10})
case("charge_path", budget={**_BUDGET, "paths_remaining": 5})
case("charge_remote_read", budget=_BUDGET)
case("charge_remote_read", budget={**_BUDGET, "remote_reads_remaining": 0})
case("charge_remote_read", budget={**_BUDGET, "remote_reads_remaining": 1})
case("charge_remote_read", budget={**_BUDGET, "remote_reads_remaining": 10})
case("charge_remote_read", budget={**_BUDGET, "remote_reads_remaining": 5})

# ── _budget_exhausted ──────────────────────────────────────────────────────
case("_budget_exhausted", budget=_BUDGET, what="text", depth=2)
case("_budget_exhausted", budget=_BUDGET, what="paths", depth=0)
case("_budget_exhausted", budget=_BUDGET, what="remote reads", depth=1)
case("_budget_exhausted", budget=_BUDGET, what="text", depth=0, io=io(**{"logger.warning": {"ok": True}}))
case("_budget_exhausted", budget=_BUDGET, what="text", depth=3)

# ── _unreadable_reason ─────────────────────────────────────────────────────
case("_unreadable_reason", path="/x.sh",
     io=io(has_live_connection=False, **{"os.stat": {"st_mode": 33188, "st_size": 10}}))
case("_unreadable_reason", path="/x.sh",
     io=io(has_live_connection=True, **{"os.stat": {"st_mode": 33188, "st_size": 10}}))
case("_unreadable_reason", path="/x.sh",
     io=io(has_live_connection=False, **{"os.stat": exc("FileNotFoundError", "missing")}))
case("_unreadable_reason", path="/x.sh",
     io=io(has_live_connection=False, **{"os.stat": {"st_mode": 16384, "st_size": 10}}))
case("_unreadable_reason", path="/x.sh",
     io=io(has_live_connection=False, **{"os.stat": {"st_mode": 33188, "st_size": 99}}))

# ── _refuse_unreadable ─────────────────────────────────────────────────────
case("_refuse_unreadable", budget=_BUDGET, path="/x.sh", reason="too big")
case("_refuse_unreadable", budget=_BUDGET, path="/y.sh", reason="device")
case("_refuse_unreadable", budget=_BUDGET, path="/z.sh", reason="")
case("_refuse_unreadable", budget=_BUDGET, path="/a.sh", reason="live sqlite")
case("_refuse_unreadable", budget=_BUDGET, path="/b.sh", reason="cloud path")

# ── _shlex_tokens ──────────────────────────────────────────────────────────
case("_shlex_tokens", line="launchctl bootout gui/1/ai.hermes.gateway", io=io(shlex_tokens=["launchctl", "bootout", "gui/1/ai.hermes.gateway"]))
case("_shlex_tokens", line="echo 'a b'", io=io(shlex_tokens=["echo", "a b"]))
case("_shlex_tokens", line="a; b", io=io(shlex_tokens=["a", ";", "b"]))
case("_shlex_tokens", line="bad 'quote", io=io(shlex_tokens=exc("ValueError", "No closing quotation")))
case("_shlex_tokens", line="", io=io(shlex_tokens=[]))

# ── _split_logical_lines ───────────────────────────────────────────────────
case("_split_logical_lines", text="a\nb\nc")
case("_split_logical_lines", text="echo 'a\nb'\nc")
case("_split_logical_lines", text="echo \"x\\\"y\"\nz")
case("_split_logical_lines", text="")
case("_split_logical_lines", text="trailing")
case("_split_logical_lines", text="a\\\\\nb")

# ── _split_segments ────────────────────────────────────────────────────────
case("_split_segments", tokens=["a", ";", "b", "|", "c"], keep_controls=False)
case("_split_segments", tokens=["a", ";", "b"], keep_controls=True)
case("_split_segments", tokens=[], keep_controls=False)
case("_split_segments", tokens=["&&", "x", "&", "y"], keep_controls=False)
case("_split_segments", tokens=["x", ">", "y"], keep_controls=True)

# ── _executed_command_index ────────────────────────────────────────────────
case("_executed_command_index", segment=["A=1", "echo", "hi"],
     io=io(command_token_index=1, peel_transparent_prefixes=1))
case("_executed_command_index", segment=["sudo", "echo", "hi"],
     io=io(command_token_index=0, peel_transparent_prefixes=1))
case("_executed_command_index", segment=["sudo"],
     io=io(command_token_index=0, peel_transparent_prefixes=1))
case("_executed_command_index", segment=[], io=io(command_token_index=None, peel_transparent_prefixes=None))
case("_executed_command_index", segment=["echo", "hi"],
     io=io(command_token_index=0, peel_transparent_prefixes=0))

# ── _is_nonlethal_signal ───────────────────────────────────────────────────
for s in ("0", "SIG0", "USR2", "SIGUSR2", "12", "TERM", "sigusr2", "9"):
    case("_is_nonlethal_signal", spec=s)

# ── _is_nonlethal_signal_option ────────────────────────────────────────────
for a in ("-0", "-s0", "--signal=0", "-USR2", "-sUSR2", "-12", "-sTERM", "0"):
    case("_is_nonlethal_signal_option", arg=a)

# ── _is_pid_operand ────────────────────────────────────────────────────────
case("_is_pid_operand", token="1234", pid=1234, group=False)
case("_is_pid_operand", token="+1234", pid=1234, group=False)
case("_is_pid_operand", token="-1234", pid=1234, group=True)
case("_is_pid_operand", token="-1234", pid=1234, group=False)
case("_is_pid_operand", token="001234", pid=1234, group=False)
case("_is_pid_operand", token="12x4", pid=1234, group=False)
case("_is_pid_operand", token="", pid=1234, group=False)

# ── _kill_targets_pid ──────────────────────────────────────────────────────
case("_kill_targets_pid", args=["-9", "1234"], pid=1234)
case("_kill_targets_pid", args=["-l"], pid=1234)
case("_kill_targets_pid", args=["-0", "1234"], pid=1234)
case("_kill_targets_pid", args=["-s", "0", "1234"], pid=1234)
case("_kill_targets_pid", args=["--", "-1234"], pid=1234)
case("_kill_targets_pid", args=["1234"], pid=1234)
case("_kill_targets_pid", args=["-s", "TERM", "1234"], pid=1234)
case("_kill_targets_pid", args=["2>/dev/null", "-0", "1234"], pid=1234)

# ── _preview_words ─────────────────────────────────────────────────────────
case("_preview_words", command="Stop-Process -Id 1 -WhatIf")
case("_preview_words", command="echo 'a b' c")
case("_preview_words", command="echo 'unbalanced")
case("_preview_words", command="")
case("_preview_words", command="a\tb  c")

# ── _is_stop_process_whatif_preview ────────────────────────────────────────
case("_is_stop_process_whatif_preview", command="Stop-Process -Id 1 -WhatIf")
case("_is_stop_process_whatif_preview", command="Stop-Process -Id 1 -wi")
case("_is_stop_process_whatif_preview", command="Stop-Process -Id 1")
case("_is_stop_process_whatif_preview", command="kill -WhatIf 1; echo x")
case("_is_stop_process_whatif_preview", command="echo -WhatIf")
case("_is_stop_process_whatif_preview", command="Stop-Process -Id 1 -WhatIf:$false")

# ── _windows_killer_targets_pid ────────────────────────────────────────────
case("_windows_killer_targets_pid", name="taskkill", args=["/PID", "1234"], pid=1234)
case("_windows_killer_targets_pid", name="taskkill", args=["/S", "host", "/PID", "1234"], pid=1234)
case("_windows_killer_targets_pid", name="stop-process", args=["-Id", "1234"], pid=1234)
case("_windows_killer_targets_pid", name="stop-process", args=["-Id", "1234"], pid=1234, whatif_preview=True)
case("_windows_killer_targets_pid", name="stop-process", args=["1234"], pid=1234)
case("_windows_killer_targets_pid", name="taskkill", args=["/PID:1234"], pid=1234)
case("_windows_killer_targets_pid", name="taskkill", args=["/PID", "9999"], pid=1234)

# ── _string_runner_payload ─────────────────────────────────────────────────
case("_string_runner_payload", name="eval", args=["kill", "1234"])
case("_string_runner_payload", name="powershell", args=["-c", "kill 1234"])
case("_string_runner_payload", name="cmd", args=["/c", "taskkill /PID 1234"])
case("_string_runner_payload", name="pwsh", args=["-NoProfile", "-c", "kill 1"])
case("_string_runner_payload", name="python", args=["-c", "x"])

# ── _targets_gateway_pid ───────────────────────────────────────────────────
_TPID = io(strip_inert_heredoc_bodies="kill 1234", iter_command_segments=[["kill", "1234"]],
           command_token_index=0, peel_transparent_prefixes=0, executable_name="kill")
case("_targets_gateway_pid", command="kill 1234", pid=1234, io=_TPID)
case("_targets_gateway_pid", command="echo hi", pid=1234,
     io=io(strip_inert_heredoc_bodies="echo hi", iter_command_segments=[["echo", "hi"]],
           command_token_index=0, peel_transparent_prefixes=0, executable_name="echo"))
case("_targets_gateway_pid", command="", pid=1234,
     io=io(strip_inert_heredoc_bodies="", iter_command_segments=[]))
case("_targets_gateway_pid", command="taskkill /PID 1234", pid=1234,
     io=io(strip_inert_heredoc_bodies="taskkill /PID 1234", iter_command_segments=[["taskkill", "/PID", "1234"]],
           command_token_index=0, peel_transparent_prefixes=0, executable_name="taskkill"))
case("_targets_gateway_pid", command="kill 9999", pid=1234,
     io=io(strip_inert_heredoc_bodies="kill 9999", iter_command_segments=[["kill", "9999"]],
           command_token_index=0, peel_transparent_prefixes=0, executable_name="kill"))

# ── path handling ──────────────────────────────────────────────────────────
case("_resolve_lenient", path="/a/b.sh", io=io(**{"path.resolve": "/a/b.sh"}))
case("_resolve_lenient", path="/a/b.sh", io=io(**{"path.resolve": exc("OSError", "bad")}))
case("_resolve_lenient", path="./b.sh", io=io(**{"path.resolve": "/w/b.sh"}))
case("_resolve_lenient", path="/x", io=io(**{"path.resolve": exc("ValueError", "nul")}))
case("_resolve_lenient", path="/x", io=io(**{"path.resolve": "/x"}))

case("_is_cloud_placeholder_path", path="/Users/me/Library/Mobile Documents/x")
case("_is_cloud_placeholder_path", path="/Users/me/Library/CloudStorage/x")
case("_is_cloud_placeholder_path", path="/Users/me/Documents/x")
case("_is_cloud_placeholder_path", path="/Library/Mobile Documents")
case("_is_cloud_placeholder_path", path="/")

case("_on_cloud_path", path="/Users/me/Library/Mobile Documents/x", io=io(**{"path.resolve": "/same"}))
case("_on_cloud_path", path="/a/b.sh", io=io(**{"path.resolve": "/Users/me/Library/CloudStorage/x"}))
case("_on_cloud_path", path="/a/b.sh", io=io(**{"path.resolve": "/a/b.sh"}))
case("_on_cloud_path", path="", io=io(**{"path.resolve": ""}))
case("_on_cloud_path", path="/Library/CloudStorage/a", io=io(**{"path.resolve": "/Library/CloudStorage/a"}))

case("_expand_candidate_path", candidate="~/scripts/x.sh", io=io(**{"path.expanduser": "/home/u/scripts/x.sh"}))
case("_expand_candidate_path", candidate="/abs/x.sh", io=io(**{"path.expanduser": "/abs/x.sh"}))
case("_expand_candidate_path", candidate="", io=io(**{"path.expanduser": ""}))
case("_expand_candidate_path", candidate="~nobody/x", io=io(**{"path.expanduser": exc("RuntimeError", "no home")}))
case("_expand_candidate_path", candidate="x", io=io(**{"path.expanduser": exc("ValueError", "nul")}))

case("_resolved_or_nothing", candidate="/abs/x.sh", cwd="/work", io=io(**{"path.expanduser": "/abs/x.sh"}))
case("_resolved_or_nothing", candidate="rel/x.sh", cwd="/work", io=io(**{"path.expanduser": "rel/x.sh"}))
case("_resolved_or_nothing", candidate="rel/x.sh", cwd=None,
     io=io(**{"path.expanduser": "rel/x.sh", "path.cwd": "/cwd"}))
case("_resolved_or_nothing", candidate="", cwd="/work", io=io(**{"path.expanduser": ""}))
case("_resolved_or_nothing", candidate="rel", cwd=None,
     io=io(**{"path.expanduser": "rel", "path.cwd": exc("OSError", "gone")}))

# ── heredoc helpers ────────────────────────────────────────────────────────
_HD = io(shlex_tokens=seq(["cat", ">", "P", "<<", "'EOF'"]), executable_name=seq("cat", "cat"),
         command_token_index=0, peel_transparent_prefixes=0)
case("_heredoc_write_targets", unit="cat > P <<'EOF'", io=_HD)
case("_heredoc_write_targets", unit="tee -a P <<'EOF'",
     io=io(shlex_tokens=seq(["tee", "-a", "P", "<<", "EOF"]), executable_name=seq("tee", "tee"),
           command_token_index=0, peel_transparent_prefixes=0))
case("_heredoc_write_targets", unit="cat <<'EOF' | tee -a P",
     io=io(shlex_tokens=seq(["cat", "<<", "EOF", "|", "tee", "-a", "P"]),
           executable_name=seq("cat", "tee", "cat", "tee"),
           command_token_index=seq(0, 3), peel_transparent_prefixes=seq(0, 3)))
case("_heredoc_write_targets", unit="echo hi",
     io=io(shlex_tokens=seq(["echo", "hi"]), executable_name=seq("echo", "echo"),
           command_token_index=0, peel_transparent_prefixes=0))
case("_heredoc_write_targets", unit="cat > P <<'EOF'",
     io=io(shlex_tokens=seq(exc("ValueError", "quote")), executable_name="cat"))

case("_collect_heredoc_written_scripts", command="cat > P <<'EOF'", cwd="/w", written={},
     io=io(iter_heredoc_bodies=[["cat > P <<'EOF'", "body one"]],
           shlex_tokens=seq(["cat", ">", "P", "<<", "EOF"]), executable_name=seq("cat", "cat"),
           command_token_index=0, peel_transparent_prefixes=0,
           **{"path.expanduser": "P", "path.resolve": "/w/P"}))
case("_collect_heredoc_written_scripts", command="echo x", cwd="/w", written={},
     io=io(iter_heredoc_bodies=[]))
case("_collect_heredoc_written_scripts", command="cat > P <<'EOF'", cwd="/w",
     written={"/w/P": ["old"]},
     io=io(iter_heredoc_bodies=[["cat > P <<'EOF'", "old"], ["cat > P <<'EOF'", "new"]],
           shlex_tokens=seq(["cat", ">", "P", "<<", "EOF"], ["cat", ">", "P", "<<", "EOF"]),
           executable_name=seq("cat", "cat", "cat", "cat"),
           command_token_index=seq(0, 0), peel_transparent_prefixes=seq(0, 0),
           **{"path.expanduser": "P", "path.resolve": "/w/P"}))
case("_collect_heredoc_written_scripts", command="cat > P <<'EOF'", cwd=None, written={},
     io=io(iter_heredoc_bodies=[["cat > P <<'EOF'", "b"]],
           shlex_tokens=seq(["cat", ">", "P", "<<", "EOF"]), executable_name=seq("cat", "cat"),
           command_token_index=0, peel_transparent_prefixes=0,
           **{"path.expanduser": "P", "path.cwd": "/cwd", "path.resolve": "/cwd/P"}))
case("_collect_heredoc_written_scripts", command="x", cwd="/w", written={},
     io=io(iter_heredoc_bodies=[["cat > P <<'EOF'", "b"]],
           shlex_tokens=seq(exc("ValueError", "bad")), executable_name="cat"))

# ── _walk_candidates ───────────────────────────────────────────────────────
case("_walk_candidates", command="bash a.sh", walk_command="bash a.sh", cwd="/w", executed=True,
     io=io(iter_referenced_shell_scripts=seq(["/w/a.sh"])))
case("_walk_candidates", command="echo hi", walk_command="echo hi", cwd="/w", executed=False,
     io=io(iter_referenced_shell_scripts=seq([])))
case("_walk_candidates", command="raw", walk_command="masked", cwd=None, executed=True,
     io=io(iter_referenced_shell_scripts=seq(["/x/a.sh"], ["/x/b.sh"])))
case("_walk_candidates", command="x", walk_command="x", cwd="/w", executed=True,
     io=io(iter_referenced_shell_scripts=seq(["/a", "/b"])))
case("_walk_candidates", command="x", walk_command="y", cwd="/w", executed=False,
     io=io(iter_referenced_shell_scripts=seq([])))

# ── _has_binary_magic ──────────────────────────────────────────────────────
case("_has_binary_magic", data="#!/bin/sh\necho hi")
case("_has_binary_magic", data="MZ\x90\x00")
case("_has_binary_magic", data="!<arch>\n")
case("_has_binary_magic", data="echo hello")
case("_has_binary_magic", data="PK\x03\x04junk")
case("_has_binary_magic", data="")

# ── _read_referenced_script_unlocked ───────────────────────────────────────
_R = {"path.resolve": "/w/a.sh"}
case("_read_referenced_script_unlocked", path="/w/a.sh", max_bytes=None,
     io=io(**{**_R, "os.open": 3, "os.fstat": {"st_mode": 33188, "st_size": 10},
              "os.read": seq("echo hi", ""), "os.close": None}))
case("_read_referenced_script_unlocked", path="/w/a.sh", max_bytes=4,
     io=io(**{**_R, "os.open": 3, "os.fstat": {"st_mode": 33188, "st_size": 100},
              "os.read": seq("echo hi", ""), "os.close": None}))
case("_read_referenced_script_unlocked", path="/w/a.sh", max_bytes=None,
     io=io(**{**_R, "os.open": 3, "os.fstat": {"st_mode": 16384, "st_size": 10},
              "os.close": None}))
case("_read_referenced_script_unlocked", path="/w/a.sh", max_bytes=None,
     io=io(**{**_R, "os.open": 3, "os.fstat": {"st_mode": 33188, "st_size": 10},
              "os.read": seq("MZbinary", ""), "os.close": None}))
case("_read_referenced_script_unlocked", path="/w/a.sh", max_bytes=None,
     io=io(**{**_R, "os.open": exc("FileNotFoundError", "gone")}))
case("_read_referenced_script_unlocked", path="/Users/me/Library/Mobile Documents/x", max_bytes=None,
     io=io(**{"path.resolve": "/same", "os.open": 3}))

# ── _scan_written_bodies ───────────────────────────────────────────────────
case("_scan_written_bodies", bodies=["body one"], cwd="/w", executed=True,
     io=io(recurse=seq(False)))
case("_scan_written_bodies", bodies=["one", "two"], cwd="/w", executed=True,
     io=io(recurse=seq(False, True)))
case("_scan_written_bodies", bodies=["", "one"], cwd="/w", executed=False,
     io=io(recurse=seq(False)))
case("_scan_written_bodies", bodies=["a", "b", "a"], cwd=None, executed=True,
     io=io(recurse=seq(False, False, False, False)))
case("_scan_written_bodies", bodies=[], cwd="/w", executed=True, io=io(recurse=[]))

# ── scan_gateway_lifecycle ─────────────────────────────────────────────────
case("scan_gateway_lifecycle", command="echo hi", cwd="/w",
     io=io(**{"_contains_unsafe_gateway_action": False}))
case("scan_gateway_lifecycle", command="hermes gateway restart", cwd="/w",
     io=io(**{"_contains_unsafe_gateway_action": True}))
case("scan_gateway_lifecycle", command="echo hi", cwd=None,
     io=io(**{"_contains_unsafe_gateway_action": exc("RuntimeError", "boom"),
              "_direct_lifecycle_scan": False}))
case("scan_gateway_lifecycle", command="hermes gateway restart", cwd=None,
     io=io(**{"_contains_unsafe_gateway_action": exc("RuntimeError", "boom"),
              "_direct_lifecycle_scan": True}))
case("scan_gateway_lifecycle", command="echo hi", cwd="/w", gateway_pid=1234,
     io=io(**{"_contains_unsafe_gateway_action": False}))
case("scan_gateway_lifecycle", command="x", cwd=None, gateway_pid=None,
     io=io(**{"_contains_unsafe_gateway_action": exc("SystemExit", "propagate")}))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT}")


if __name__ == "__main__":
    main()
