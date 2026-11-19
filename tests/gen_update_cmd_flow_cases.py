#!/usr/bin/env python3
"""Generate tests/fixtures/update_cmd_flow.jsonl — scripted-seam FLOW cases for
the update_cmd_flow port (hermes_cli/update_cmd.py UNCLAIMED update pipeline:
retired shims, config/receipt leaves, git plumbing, channel/resume).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides.

Domains (documented in the port header):
- ASCII strings, POSIX paths; git_cmd/args/cmd arrays of plain ASCII strings.
- run_git scripts are complete {returncode, stdout, stderr} records or raises.
- subprocess raise specs carry cmd/timeout/returncode/stderr fields.
- args/broken are objects-or-null; opts is always an object; snapshot_id str|null.
Run: python tests/gen_update_cmd_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "update_cmd_flow.jsonl")
CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def rg(rc=0, stdout="", stderr=""):
    return {"returncode": rc, "stdout": stdout, "stderr": stderr}


GIT = ["git"]
# base seams for a _git_run with cwd=None (project_root) and non-network.
GRUN = {"project_root": "/repo", "windows_hide_flags": 0}


def grun(run_git, **over):
    s = dict(GRUN)
    s["run_git"] = run_git
    s.update(over)
    return s


# ── get_default_hermes_root ──
HOOK = "hermes_cli.update_cmd.get_default_hermes_root"
add("get_default_hermes_root", {}, {"stop_historical_updater": rs("RetiredUpdaterHook", HOOK + " is a retired updater hook; it does nothing outside a running update.")})
add("get_default_hermes_root", {}, {"stop_historical_updater": rs("SystemExit", "1")})
add("get_default_hermes_root", {}, {"stop_historical_updater": rs("KeyboardInterrupt", "")})
add("get_default_hermes_root", {}, {"stop_historical_updater": None})
add("get_default_hermes_root", {}, {"stop_historical_updater": rs("RuntimeError", "boom")})
add("get_default_hermes_root", {}, {})  # fail-closed

# ── ensure_venv_pip ──
for i, sc in enumerate([rs("RetiredUpdaterHook", "retired"), rs("SystemExit", "1"),
                        rs("KeyboardInterrupt", ""), None, rs("OSError", "x")]):
    add("ensure_venv_pip", {"pip_cmd": ["pip"], "python_exe": "/py%d" % i},
        {"stop_historical_updater": sc})

# ── pip_install_prefix ──
for i, sc in enumerate([rs("RetiredUpdaterHook", "retired"), rs("SystemExit", "2"),
                        rs("GeneratorExit", ""), None, rs("ValueError", "x")]):
    add("pip_install_prefix", {"uv_bin": "/uv%d" % i}, {"stop_historical_updater": sc})

# ── updates_config ──
add("updates_config", {}, {"load_config": {"updates": {"a": 1}}})
add("updates_config", {}, {"load_config": {}})
add("updates_config", {}, {"load_config": None})
add("updates_config", {}, {"load_config": {"updates": None}})
add("updates_config", {}, {"load_config": {"updates": "x"}})
add("updates_config", {}, {"load_config": {"updates": {"parked_branch_strategy": "switch"}}})
add("updates_config", {}, {"load_config": "x"})
add("updates_config", {}, {"load_config": 5})
add("updates_config", {}, {"load_config": [1]})
add("updates_config", {}, {"load_config": rs("RuntimeError", "cfg boom")})
add("updates_config", {}, {"load_config": rs("KeyboardInterrupt", "")})
add("updates_config", {}, {})  # fail-closed

# ── record_update_step ──
add("record_update_step", {"step": "fetch", "ok": True, "detail": "d"}, {"record_step": None})
add("record_update_step", {"step": "fetch", "ok": False}, {"record_step": rs("RuntimeError", "x")})
add("record_update_step", {"step": "s", "ok": True, "detail": ""}, {"record_step": rs("ValueError", "x")})
add("record_update_step", {"step": "s", "ok": True}, {"record_step": rs("KeyboardInterrupt", "")})
add("record_update_step", {"step": "s", "ok": False}, {"record_step": rs("SystemExit", "1")})
add("record_update_step", {"step": "s", "ok": True}, {"record_step": rs("OSError", "x")})
add("record_update_step", {"step": "s", "ok": True}, {})  # fail-closed (suppressed)

# ── record_update_skip ──
add("record_update_skip", {"step": "backup", "reason": "off"}, {"record_skip": None})
add("record_update_skip", {"step": "b", "reason": "r"}, {"record_skip": rs("RuntimeError", "x")})
add("record_update_skip", {"step": "b", "reason": "r"}, {"record_skip": rs("ValueError", "x")})
add("record_update_skip", {"step": "b", "reason": "r"}, {"record_skip": rs("KeyboardInterrupt", "")})
add("record_update_skip", {"step": "b", "reason": "r"}, {"record_skip": rs("SystemExit", "1")})
add("record_update_skip", {"step": "b", "reason": "r"}, {})  # fail-closed (suppressed)

# ── record_pre_update_backup_outcome ──
add("record_pre_update_backup_outcome", {"snapshot_id": "snap1"}, {"record_step": None})
add("record_pre_update_backup_outcome", {"snapshot_id": None, "args": {"no_backup": True}},
    {"resolve_pre_update_backup_mode": "off", "record_skip": None})
add("record_pre_update_backup_outcome", {"snapshot_id": None, "args": {}},
    {"resolve_pre_update_backup_mode": "off", "record_skip": None})
add("record_pre_update_backup_outcome", {"snapshot_id": None, "args": {}},
    {"resolve_pre_update_backup_mode": "on", "record_step": None})
add("record_pre_update_backup_outcome", {"snapshot_id": "", "args": {}},
    {"resolve_pre_update_backup_mode": "on", "record_step": None})
add("record_pre_update_backup_outcome", {"snapshot_id": None, "args": {}},
    {"resolve_pre_update_backup_mode": rs("RuntimeError", "x")})
add("record_pre_update_backup_outcome", {"snapshot_id": "snap1"},
    {"record_step": rs("KeyboardInterrupt", "")})
add("record_pre_update_backup_outcome", {"snapshot_id": None},
    {"resolve_pre_update_backup_mode": "off", "record_skip": None})

# ── record_snapshot_stage ──
add("record_snapshot_stage", {"snapshot_id": "s"}, {"record_stage": None})
add("record_snapshot_stage", {"snapshot_id": None, "args": {}},
    {"resolve_pre_update_backup_mode": "off", "record_stage": None})
add("record_snapshot_stage", {"snapshot_id": None, "args": {}},
    {"resolve_pre_update_backup_mode": "on", "record_stage": None})
add("record_snapshot_stage", {"snapshot_id": "", "args": {}},
    {"resolve_pre_update_backup_mode": "off", "record_stage": None})
add("record_snapshot_stage", {"snapshot_id": None, "args": {}},
    {"resolve_pre_update_backup_mode": rs("RuntimeError", "x")})
add("record_snapshot_stage", {"snapshot_id": "s"}, {"record_stage": rs("SystemExit", "1")})

# ── finalize_receipt ──
add("finalize_receipt", {"status": "failed", "debug_message": "m"}, {"finalize_update_receipt": None})
add("finalize_receipt", {"status": "success", "debug_message": "m"}, {"finalize_update_receipt": rs("RuntimeError", "x")})
add("finalize_receipt", {"status": "failed", "debug_message": "m"}, {"finalize_update_receipt": rs("ValueError", "x")})
add("finalize_receipt", {"status": "failed", "debug_message": "m"}, {"finalize_update_receipt": rs("KeyboardInterrupt", "")})
add("finalize_receipt", {"status": "failed", "debug_message": "m"}, {"finalize_update_receipt": rs("SystemExit", "1")})
add("finalize_receipt", {"status": "failed", "debug_message": "m"}, {"finalize_update_receipt": rs("OSError", "x")})
add("finalize_receipt", {"status": "failed", "debug_message": "m"}, {})  # fail-closed (swallowed)

# ── git_run ──
add("git_run", {"git_cmd": GIT, "args": ["status"], "cwd": "/repo"},
    {"windows_hide_flags": 0, "run_git": rg(0, "ok", "")})
add("git_run", {"git_cmd": GIT, "args": ["fetch"], "cwd": "/repo", "network": True},
    {"no_prompt_git_kwargs": {}, "windows_hide_flags": 0, "run_git": rg(0, "", "")})
add("git_run", {"git_cmd": GIT, "args": ["fetch"], "cwd": "/repo", "network": True},
    {"no_prompt_git_kwargs": {"env": {"GIT_TERMINAL_PROMPT": "0"}}, "windows_hide_flags": 0, "run_git": rg(0, "", "")})
add("git_run", {"git_cmd": GIT, "args": ["status"]},
    grun(rg(0, "ok", "")))
add("git_run", {"git_cmd": GIT, "args": ["fetch", "origin"], "cwd": "/repo"},
    {"windows_hide_flags": 0, "run_git": {"__raise__": "TimeoutExpired", "cmd": ["git", "fetch"], "timeout": 300}})
add("git_run", {"git_cmd": GIT, "args": ["fetch", "origin"], "cwd": "/repo", "check": True},
    {"windows_hide_flags": 0, "run_git": {"__raise__": "TimeoutExpired", "cmd": ["git", "fetch"], "timeout": 300}})
add("git_run", {"git_cmd": GIT, "args": ["status"], "cwd": "/repo"},
    {"windows_hide_flags": 0, "run_git": rs("OSError", "disk")})
add("git_run", {"git_cmd": GIT, "args": ["status"], "cwd": "/repo"},
    {"windows_hide_flags": 0, "run_git": rs("KeyboardInterrupt", "")})
add("git_run", {"git_cmd": GIT, "args": ["status"], "cwd": "/repo"},
    {"windows_hide_flags": rs("RuntimeError", "x")})
add("git_run", {"git_cmd": GIT, "args": ["status"], "cwd": "/repo"},
    {"windows_hide_flags": 0})  # fail-closed run_git
add("git_run", {"git_cmd": GIT, "args": ["fetch"], "cwd": "/repo", "network": True, "check": True},
    {"no_prompt_git_kwargs": {"creationflags": 99}, "windows_hide_flags": 0, "run_git": rg(0, "", "")})

# ── base_git_cmd ──
for plat in ["win32", "linux", "darwin", "", "freebsd"]:
    add("base_git_cmd", {"platform": plat}, {})

# ── is_shallow_checkout ──
add("is_shallow_checkout", {"git_cmd": GIT}, grun(rg(0, "true", "")))
add("is_shallow_checkout", {"git_cmd": GIT}, grun(rg(0, "false", "")))
add("is_shallow_checkout", {"git_cmd": GIT}, grun(rg(0, " true \n", "")))
add("is_shallow_checkout", {"git_cmd": GIT}, grun(rg(0, "", "")))
add("is_shallow_checkout", {"git_cmd": GIT}, grun(rg(0, None, "")))
add("is_shallow_checkout", {"git_cmd": GIT}, grun(rs("OSError", "x")))

# ── tip_shas ──
add("tip_shas", {"git_cmd": GIT, "target_ref": "origin/main"},
    grun([rg(0, "aaa", ""), rg(0, "bbb", "")]))
add("tip_shas", {"git_cmd": GIT, "target_ref": "origin/main", "base": "feature"},
    grun([rg(0, "aaa", ""), rg(0, "bbb", "")]))
add("tip_shas", {"git_cmd": GIT, "target_ref": "origin/main"},
    grun([rg(0, "aaa", ""), rg(0, None, "")]))
add("tip_shas", {"git_cmd": GIT, "target_ref": "origin/main"},
    grun([rs("OSError", "x"), rg(0, "bbb", "")]))
add("tip_shas", {"git_cmd": GIT, "target_ref": "origin/main"},
    grun([rg(0, "", ""), rg(0, "", "")]))

# ── resolved_commit ──
add("resolved_commit", {"git_cmd": GIT, "ref": "origin/main"}, grun(rg(0, "abc123\n", "")))
add("resolved_commit", {"git_cmd": GIT, "ref": "origin/main"}, grun(rg(0, "", "")))
add("resolved_commit", {"git_cmd": GIT, "ref": "origin/main"}, grun(rg(0, None, "")))
add("resolved_commit", {"git_cmd": GIT, "ref": "HEAD"}, grun(rg(1, "", "")))
add("resolved_commit", {"git_cmd": GIT, "ref": "origin/main"}, grun(rs("OSError", "x")))

# ── current_branch_name ──
add("current_branch_name", {"git_cmd": GIT}, grun(rg(0, "main\n", "")))
add("current_branch_name", {"git_cmd": GIT}, grun(rg(0, "HEAD", "")))
add("current_branch_name", {"git_cmd": GIT, "check": True}, grun(rg(0, "main", "")))
add("current_branch_name", {"git_cmd": GIT}, grun(rg(0, None, "")))
add("current_branch_name", {"git_cmd": GIT},
    grun({"__raise__": "CalledProcessError", "returncode": 1, "cmd": ["git"],
          "msg": "Command '['git']' returned non-zero exit status 1."}))

# ── landed_off_target ──
add("landed_off_target", {"git_cmd": GIT, "target_sha": "abc"}, grun(rg(0, "abc", "")))
add("landed_off_target", {"git_cmd": GIT, "target_sha": "abc"},
    grun([rg(0, "def", ""), rg(0, "", "")]))
add("landed_off_target", {"git_cmd": GIT, "target_sha": "abc"},
    grun([rg(0, "def", ""), rg(1, "", "")]))
add("landed_off_target", {"git_cmd": GIT, "target_sha": "abc", "merged": False},
    grun(rg(0, "def", "")))
add("landed_off_target", {"git_cmd": GIT, "target_sha": "abc"}, grun(rg(0, "", "")))
add("landed_off_target", {"git_cmd": GIT, "target_sha": "abc"}, grun(rg(0, None, "")))
add("landed_off_target", {"git_cmd": GIT, "target_sha": "abc"}, grun(rs("OSError", "x")))

# ── update_movement_baseline ──
add("update_movement_baseline", {"git_cmd": GIT, "pre_pull_sha": "p", "pre_sync_sha": "s",
                                 "rollback_branch": None, "target_sha": "t"}, {})
add("update_movement_baseline", {"git_cmd": GIT, "pre_pull_sha": "p", "pre_sync_sha": "s",
                                 "rollback_branch": "feature", "target_sha": "t"},
    grun(rg(1, "", "")))
add("update_movement_baseline", {"git_cmd": GIT, "pre_pull_sha": "p", "pre_sync_sha": "s",
                                 "rollback_branch": "feature", "target_sha": "t"},
    grun(rg(0, "", "")))
add("update_movement_baseline", {"git_cmd": GIT, "pre_pull_sha": "", "pre_sync_sha": None,
                                 "rollback_branch": "feature", "target_sha": "t"}, {})
add("update_movement_baseline", {"git_cmd": GIT, "pre_pull_sha": "p", "pre_sync_sha": None,
                                 "rollback_branch": "feature", "target_sha": ""}, {})
add("update_movement_baseline", {"git_cmd": GIT, "pre_pull_sha": None, "pre_sync_sha": None,
                                 "rollback_branch": None, "target_sha": "t"}, {})
add("update_movement_baseline", {"git_cmd": GIT, "pre_pull_sha": "p", "pre_sync_sha": "s",
                                 "rollback_branch": "feature", "target_sha": "t"},
    grun(rs("OSError", "x")))

# ── missing_branch_stop ──
add("missing_branch_stop", {"git_cmd": GIT, "branch": "main"},
    grun([rg(0, "abc", ""), rg(0, "def", "")]))
add("missing_branch_stop", {"git_cmd": GIT, "branch": "main"},
    grun([rg(0, "", ""), rg(0, "def", "")]))
add("missing_branch_stop", {"git_cmd": GIT, "branch": "main"},
    grun([rg(0, "", ""), rg(0, "", "")]))
add("missing_branch_stop", {"git_cmd": GIT, "branch": "main"},
    grun([rs("OSError", "x"), rg(0, "", "")]))
add("missing_branch_stop", {"git_cmd": GIT, "branch": "main"},
    grun([rs("KeyboardInterrupt", ""), rg(0, "", "")]))
add("missing_branch_stop", {"git_cmd": GIT, "branch": "main"},
    grun([rs("RuntimeError", "x"), rg(0, "", "")]))

# ── refuse_unselected_head ──
add("refuse_unselected_head", {"git_cmd": GIT, "target_sha": "abc", "unmoved": None},
    grun(rg(0, "abc", "")))
add("refuse_unselected_head", {"git_cmd": GIT, "target_sha": "abc", "unmoved": "def"},
    dict(grun([rg(0, "def", ""), rg(1, "", "")]), print=None, record_stop=None))
add("refuse_unselected_head", {"git_cmd": GIT, "target_sha": "abc", "unmoved": "xyz"},
    dict(grun([rg(0, "def", ""), rg(1, "", "")]), print=None, record_stop=None))
add("refuse_unselected_head", {"git_cmd": GIT, "target_sha": "abc", "unmoved": None},
    dict(grun(rg(0, "", "")), print=None, record_stop=None))
add("refuse_unselected_head", {"git_cmd": GIT, "target_sha": "abc", "unmoved": None},
    grun(rs("OSError", "x")))
add("refuse_unselected_head", {"git_cmd": GIT, "target_sha": "abc", "unmoved": "xyz"},
    dict(grun([rg(0, "def", ""), rg(1, "", "")]), print=rs("OSError", "x"), record_stop=None))

# ── verify_head_after_pull ──
add("verify_head_after_pull", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": "abc"},
    {"project_root": "/r", "capture_head_sha": "abc", "print": None,
     "disarm_commit_obligations": None, "resume_windows_gateways_after_update": None,
     "record_stop": None})
add("verify_head_after_pull", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": "abc"},
    dict(grun(rg(0, "main", "")), capture_head_sha="def", print=None))
add("verify_head_after_pull", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": "abc"},
    dict(grun(rg(0, "feature", "")), capture_head_sha="def", print=None,
         resume_windows_gateways_after_update=None, record_stop=None))
add("verify_head_after_pull", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": "abc",
                               "in_place_update": True},
    dict(grun(rg(0, "feature", "")), capture_head_sha="def", print=None))
add("verify_head_after_pull", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": None},
    dict(grun(rg(0, "main", "")), capture_head_sha="def", print=None))
add("verify_head_after_pull", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": "abc"},
    {"project_root": "/r", "capture_head_sha": rs("OSError", "x")})
add("verify_head_after_pull", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": "abc"},
    dict(grun(rg(0, "HEAD", "")), capture_head_sha="def", print=None))
add("verify_head_after_pull", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": "abc"},
    dict(grun(rg(0, "", "")), capture_head_sha="def", print=None))

# ── map_ssl_cert_file_for_git ──
add("map_ssl_cert_file_for_git", {"git_cmd": GIT}, {"environ_get": None})
add("map_ssl_cert_file_for_git", {"git_cmd": GIT}, {"environ_get": ["/ca.pem", "/existing"]})
add("map_ssl_cert_file_for_git", {"git_cmd": GIT},
    dict(grun(rg(0, "value", "")), environ_get=["/ca.pem", None]))
add("map_ssl_cert_file_for_git", {"git_cmd": GIT},
    dict(grun(rg(0, "", "")), environ_get=["/ca.pem", None], environ_set=None))
add("map_ssl_cert_file_for_git", {"git_cmd": GIT},
    dict(grun(rg(1, "", "")), environ_get=["/ca.pem", None], environ_set=None))
add("map_ssl_cert_file_for_git", {"git_cmd": GIT}, {"environ_get": ""})
add("map_ssl_cert_file_for_git", {"git_cmd": GIT}, {"environ_get": rs("OSError", "x")})
add("map_ssl_cert_file_for_git", {"git_cmd": GIT},
    dict(grun(rg(0, None, "")), environ_get=["/ca.pem", None]))

# ── heal_stale_shallow_checkout ──
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {}, "heal_shallow_history": True, "print": None})
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {}, "heal_shallow_history": False})
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {}, "heal_shallow_history": rs("OSError", "disk full"), "print": None})
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {}, "heal_shallow_history": {"__raise__": "CalledProcessError",
     "returncode": 128, "cmd": ["git", "fetch"], "stderr": "fatal: bad\nsecond line"}, "print": None})
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {}, "heal_shallow_history": {"__raise__": "TimeoutExpired",
     "cmd": ["git", "fetch"], "timeout": 300}, "print": None})
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {}, "heal_shallow_history": rs("SubprocessError", ""), "print": None})
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {}, "heal_shallow_history": rs("RuntimeError", "x")})
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {}, "heal_shallow_history": rs("KeyboardInterrupt", "")})
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {"env": {"GIT_TERMINAL_PROMPT": "0"}}, "heal_shallow_history": True, "print": None})
add("heal_stale_shallow_checkout", {"repo_root": "/r", "branch": "main"},
    {"no_prompt_git_kwargs": {}, "heal_shallow_history": {"__raise__": "CalledProcessError",
     "returncode": 128, "cmd": ["git"], "stderr": ""}, "print": None})

# ── source_update_channel ──
add("source_update_channel", {"branch_explicit": True}, {})
add("source_update_channel", {"args": {"branch": "feature"}}, {})
add("source_update_channel", {"channel": "stable"}, {"validate_name": "stable"})
add("source_update_channel", {"args": {"channel": "beta"}}, {"validate_name": "beta"})
add("source_update_channel", {"args": {}},
    {"get_config_path": "/cfg", "require_readable_config_before_write": {"updates": {}},
     "project_root": "/r", "resolve_update_channel": "main"})
add("source_update_channel", {"channel": "bad"}, {"validate_name": rs("ChannelError", "bad name")})
add("source_update_channel", {"args": {"branch": None}},
    {"get_config_path": "/cfg", "require_readable_config_before_write": {},
     "project_root": "/r", "resolve_update_channel": "main"})
add("source_update_channel", {},
    {"get_config_path": "/cfg", "require_readable_config_before_write": {},
     "project_root": "/r", "resolve_update_channel": "main"})
add("source_update_channel", {"channel": ""}, {"validate_name": ""})
add("source_update_channel", {"args": {}},
    {"get_config_path": "/cfg", "require_readable_config_before_write": {},
     "project_root": "/r", "resolve_update_channel": rs("RuntimeError", "x")})

# ── update_run_channel ──
add("update_run_channel", {"args": {"branch": "x"}}, {})
add("update_run_channel", {"args": {"channel": "stable"}}, {"validate_name": "stable"})
add("update_run_channel", {"args": {"channel": "bad"}},
    {"validate_name": rs("ChannelError", "bad"), "record_stop": None})
add("update_run_channel", {"args": {"channel": "bad"}},
    {"validate_name": rs("ValueError", "bad"), "record_stop": None})
add("update_run_channel", {"args": {"channel": "bad"}},
    {"validate_name": rs("KeyboardInterrupt", "")})
add("update_run_channel", {"args": {}},
    {"get_config_path": "/cfg", "require_readable_config_before_write": {},
     "project_root": "/r", "resolve_update_channel": "main"})

# ── settle_windows_resume ──
add("settle_windows_resume", {"request": {}}, {"atexit_unregister": None})
add("settle_windows_resume", {"request": {"windows_resume": {"resume_needed": True}}},
    {"atexit_unregister": None})
add("settle_windows_resume", {"request": None}, {"atexit_unregister": None})
add("settle_windows_resume", {"request": {}}, {"atexit_unregister": rs("RuntimeError", "x")})
add("settle_windows_resume", {"request": {}}, {"atexit_unregister": [None, rs("SystemExit", "1")]})
add("settle_windows_resume", {"request": {"a": 1}}, {"atexit_unregister": None})

# ── resume_paused_gateways_at_exit ──
add("resume_paused_gateways_at_exit", {"token": None, "request": None}, {})
add("resume_paused_gateways_at_exit", {"token": {"resume_needed": False}, "request": None}, {})
add("resume_paused_gateways_at_exit", {"token": {"resume_needed": True},
                                       "request": {"windows_resume_settled": True}}, {})
add("resume_paused_gateways_at_exit", {"token": {"resume_needed": True}, "request": None},
    {"resume_windows_gateways_after_update": None})
add("resume_paused_gateways_at_exit", {"token": {"resume_needed": True},
                                       "request": {"receipt": {"update_id": "u1"}}},
    {"resume_windows_gateways_after_update": rs("RuntimeError", "boom"), "owe_followup": None})
add("resume_paused_gateways_at_exit", {"token": {"resume_needed": True}, "request": None},
    {"resume_windows_gateways_after_update": rs("RuntimeError", "boom"), "owe_followup": None})
add("resume_paused_gateways_at_exit", {"token": {"resume_needed": True}, "request": None},
    {"resume_windows_gateways_after_update": rs("SystemExit", "1")})
add("resume_paused_gateways_at_exit", {"token": {"resume_needed": True}, "request": None},
    {"resume_windows_gateways_after_update": rs("KeyboardInterrupt", "")})
add("resume_paused_gateways_at_exit", {"token": {"resume_needed": True}, "request": {}},
    {"resume_windows_gateways_after_update": rs("RuntimeError", "x"), "owe_followup": None})
add("resume_paused_gateways_at_exit", {"token": {"resume_needed": True},
                                       "request": {"receipt": {}}},
    {"resume_windows_gateways_after_update": rs("RuntimeError", "x")})
add("resume_paused_gateways_at_exit", {"token": "x", "request": None}, {})

# ── pause_gateways_for_update ──
OPTS = {"no_gateway_restart": False, "force": False}
add("pause_gateways_for_update", {"opts": OPTS},
    {"pause_windows_gateways_for_update": {"resume_needed": True}})
add("pause_gateways_for_update", {"opts": OPTS},
    {"pause_windows_gateways_for_update": None, "arm_pause": {"armed": True}})
add("pause_gateways_for_update", {"opts": OPTS},
    {"pause_windows_gateways_for_update": None, "arm_pause": None})
add("pause_gateways_for_update", {"opts": OPTS},
    {"pause_windows_gateways_for_update": rs("RuntimeError", "x"), "record_stop": None})
add("pause_gateways_for_update", {"opts": OPTS},
    {"pause_windows_gateways_for_update": rs("RetiredUpdaterHook", "x"), "record_stop": None})
add("pause_gateways_for_update", {"opts": OPTS},
    {"pause_windows_gateways_for_update": rs("ValueError", "x")})
add("pause_gateways_for_update", {"opts": OPTS},
    {"pause_windows_gateways_for_update": rs("KeyboardInterrupt", "")})
add("pause_gateways_for_update", {"opts": OPTS},
    {"pause_windows_gateways_for_update": None, "arm_pause": rs("RuntimeError", "x"), "record_stop": None})
add("pause_gateways_for_update", {"opts": {}}, {"pause_windows_gateways_for_update": None})
add("pause_gateways_for_update", {"opts": {"no_gateway_restart": True, "force": True}},
    {"pause_windows_gateways_for_update": "", "arm_pause": {"armed": True}})

# ── upstream_broken_header ──
add("upstream_broken_header", {"broken": {"sha": "abcdefghijk"}}, {})
add("upstream_broken_header", {"broken": {"sha": "abc"}}, {})
add("upstream_broken_header", {"broken": {"sha": "0123456789abcdef"}}, {})
add("upstream_broken_header", {"broken": None}, {})
add("upstream_broken_header", {"broken": {}}, {})
add("upstream_broken_header", {"broken": {"sha": None}}, {})
add("upstream_broken_header", {"broken": {"sha": ""}}, {})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()