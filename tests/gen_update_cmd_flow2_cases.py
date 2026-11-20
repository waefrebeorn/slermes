#!/usr/bin/env python3
"""Generate tests/fixtures/update_cmd_flow2.jsonl — scripted-seam FLOW cases for
the update_cmd_flow2 port (hermes_cli/update_cmd.py REMAINING UNCLAIMED update
pipeline: completion, divergence/rollback, pull/switch, checkout prep,
options/receipt/git, error handler).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides.

Domains (documented in the port header): ASCII strings, POSIX paths; git_run
scripts are complete {returncode, stdout, stderr} records or raises; attribute
carriers (opts/args/plan/track_result) are objects-or-null; dict carriers
(request/result/token) are objects-or-null; counts fit a 64-bit int.
Run: python tests/gen_update_cmd_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "update_cmd_flow2.jsonl")
CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def rg(rc=0, stdout="", stderr=""):
    return {"returncode": rc, "stdout": stdout, "stderr": stderr}


def broken(sha, path, error):
    return {"__raise__": "UpstreamTargetBroken",
            "msg": "upstream/main (%s) has a syntax error in %s" % (sha[:10], path),
            "sha": sha, "path": path, "error": error}


def cpe(rc, cmd, output=None, stderr=None):
    return {"__raise__": "CalledProcessError",
            "msg": "Command '%s' returned non-zero exit status %d." % (cmd, rc),
            "returncode": rc, "cmd": cmd, "output": output, "stderr": stderr}


GIT = ["git"]
ROOT = "/repo"
SHA = "a" * 40
SHA2 = "b" * 40
SHA3 = "c" * 40


def cur(data=None, token="tok"):
    return {"data": data if data is not None else {"update_id": "u1"}, "current_token": token}


def result(exit_code=0, receipt=None, windows_resume=None, pm_receipt=None, error=None):
    r = {"exit_code": exit_code, "receipt": receipt, "windows_resume": windows_resume, "pm_receipt": pm_receipt}
    if error is not None:
        r["error"] = error
    return r


# ── source_completion_request ──
OPTS = {"assume_yes": True, "pre_update_version": "1.0", "no_gateway_restart": False}
base_scr = {
    "receipt_current_get": cur(), "project_root": ROOT, "path_resolve": ROOT,
    "get_hermes_home": "/home/u/.hermes", "last_sibling_snapshots": [], "plan_to_dict": {"k": 1},
}
add("source_completion_request", {"opts": OPTS, "plan": {"runtimes": []}, "snapshot_id": "snap1",
                                  "windows_resume": None, "desktop": False, "gateway_mode": False}, dict(base_scr))
add("source_completion_request", {"opts": OPTS, "plan": None, "snapshot_id": None,
                                  "windows_resume": {"resume_needed": True}, "desktop": True, "gateway_mode": True},
    dict(base_scr, last_sibling_snapshots=[{"p": "x"}]))
add("source_completion_request", {"opts": OPTS, "plan": {"runtimes": []}, "snapshot_id": "s",
                                  "windows_resume": None, "desktop": False, "gateway_mode": False},
    dict(base_scr, receipt_current_get=[None, cur()]))  # begin then re-get
add("source_completion_request", {"opts": {"pre_update_version": "1.0"}, "plan": None, "snapshot_id": None,
                                  "windows_resume": None, "desktop": False, "gateway_mode": False},
    dict(base_scr, receipt_current_get=[None, cur()]))  # missing assume_yes -> AttributeError
add("source_completion_request", {"opts": OPTS, "plan": None, "snapshot_id": None,
                                  "windows_resume": None, "desktop": False, "gateway_mode": False},
    dict(base_scr, project_root=rs("RuntimeError", "no root")))
add("source_completion_request", {"opts": OPTS, "plan": None, "snapshot_id": None,
                                  "windows_resume": None, "desktop": False, "gateway_mode": False},
    {k: v for k, v in base_scr.items() if k != "get_hermes_home"})  # fail-closed
add("source_completion_request", {"opts": OPTS, "plan": None, "snapshot_id": None,
                                  "windows_resume": None, "desktop": False, "gateway_mode": False},
    dict(base_scr, receipt_current_get=[None, None]))  # current.data on None -> AttributeError

# ── complete_source_update ──
REQ = {"receipt": {"update_id": "u1"}, "windows_resume": None, "expected_sha": SHA}
cs_base = {
    "unrestored_autostash_notice": None, "record_stage": None, "publish_stage": None,
    "receipt_current_get": cur(), "write_fleet_restart_pending_marker": None,
    "run_completion": result(0), "accept_completion_pm_receipt": None, "adopt_retired_channel": False,
}
add("complete_source_update", {"request": dict(REQ)}, dict(cs_base))
add("complete_source_update", {"request": dict(REQ, expected_sha=None)},
    dict(cs_base, current_checkout_sha=SHA2))  # or-chain fires current_checkout_sha
add("complete_source_update", {"request": dict(REQ)},
    dict(cs_base, unrestored_autostash_notice="stash left"))  # completion_message set
add("complete_source_update", {"request": dict(REQ)},
    dict(cs_base, run_completion=result(130)))  # interrupted receipt
add("complete_source_update", {"request": dict(REQ)},
    dict(cs_base, run_completion=result(1), settle_lost_completion=result(0)))  # lost completion
add("complete_source_update", {"request": dict(REQ)},
    dict(cs_base, run_completion=rs("KeyboardInterrupt", "")))  # -> SystemExit 130
add("complete_source_update", {"request": dict(REQ)},
    dict(cs_base, run_completion=rs("RuntimeError", "boom"), settle_lost_completion=result(0)))
add("complete_source_update", {"request": dict(REQ)},
    dict(cs_base, run_completion=rs("SystemExit", "3")))  # BaseException passthrough
add("complete_source_update", {"request": dict(REQ, windows_resume={"resume_needed": True})},
    dict(cs_base, run_completion=result(0, windows_resume={"resume_needed": False, "done": 1}),
         settle_windows_resume=None))  # token clear/update + settle
add("complete_source_update", {"request": dict(REQ, windows_resume={"resume_needed": True})},
    dict(cs_base, run_completion=result(0), resume_windows_gateways_after_update=None,
         settle_windows_resume=None))  # A6 resume branch
add("complete_source_update", {"request": dict(REQ, windows_resume={"resume_needed": True})},
    dict(cs_base, run_completion=result(0), resume_windows_gateways_after_update=rs("RuntimeError", "x"),
         owe_followup=None, settle_windows_resume=None))
add("complete_source_update", {"request": dict(REQ)},
    dict(cs_base, run_completion=result(0, receipt={"update_id": "u2"}),
         adopt_terminal_receipt=None, receipt_current_reset=None))  # adopt terminal
add("complete_source_update", {"request": dict(REQ)},
    dict(cs_base, adopt_retired_channel=True))  # print channel move (needs channel_retirement)
add("complete_source_update", {"request": dict(REQ, channel_retirement={"destination": "stable"})},
    dict(cs_base, adopt_retired_channel=True))
add("complete_source_update", {"request": dict(REQ)},
    dict(cs_base, adopt_retired_channel=rs("RuntimeError", "adopt fail"), owe_followup=None))
add("complete_source_update", {"request": None},
    {"unrestored_autostash_notice": None, "stop_for_relaunch": rs("SystemExit", "0")})
add("complete_source_update", {"request": dict(REQ)},
    {k: v for k, v in cs_base.items() if k != "record_stage"})  # fail-closed

# ── reconcile_diverged_checkout ──
rc_base = {"project_root": ROOT, "time_strftime": "20260101-000000", "utc_now_strftime": "20260101-000000",
           "prune_orphan_rescue_refs": None, "record_stop": None}
# custom branch merge success
add("reconcile_diverged_checkout", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": SHA, "target_ref": None},
    dict(rc_base, git_run=[rg(0, "feature"), rg(0), rg(0)]))  # show-current, tag, merge
# custom branch merge conflict
add("reconcile_diverged_checkout", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": SHA, "target_ref": None},
    dict(rc_base, git_run=[rg(0, "feature"), rg(0), rg(1), rg(0)]))  # +merge --abort
# same branch, common ancestor, backup ok
add("reconcile_diverged_checkout", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": SHA, "target_ref": None},
    dict(rc_base, git_run=[rg(0, ""), rg(0, "base"), rg(0), rg(0, "5"), rg(0)]))
# same branch, orphan, backup fail
add("reconcile_diverged_checkout", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": SHA, "target_ref": None},
    dict(rc_base, git_run=[rg(0, ""), rg(1, ""), rg(1), rg(0)]))
# reset failure
add("reconcile_diverged_checkout", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": SHA, "target_ref": None},
    dict(rc_base, git_run=[rg(0, ""), rg(1, ""), rg(0), rg(1, "", "reset err")]))
# no pre_pull_sha
add("reconcile_diverged_checkout", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": None, "target_ref": None},
    dict(rc_base, git_run=[rg(0, ""), rg(0, "base"), rg(0)]))
# target_ref explicit
add("reconcile_diverged_checkout", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": SHA, "target_ref": SHA2},
    dict(rc_base, git_run=[rg(0, ""), rg(0, "base"), rg(0), rg(0, "2"), rg(0)]))
add("reconcile_diverged_checkout", {"git_cmd": GIT, "branch": "main", "pre_pull_sha": SHA, "target_ref": None},
    {k: v for k, v in rc_base.items() if k != "git_run"})  # fail-closed

# ── rollback_if_pulled_syntax_error ──
rb_base = {"project_root": ROOT, "capture_head_sha": SHA2, "arm_tree_move": None,
           "checkout_move.enter": None, "checkout_move.exit": None, "files_added_by": [],
           "drop_added_files": True, "tree_whole_at": True, "settle_failed_tree_move": False,
           "interrupted_pull_marker": ROOT + "/.git/interrupted-pull", "marker_unlink": None,
           "marker_is_file": False, "disarm_commit_obligations": None, "owe_restore_of": None,
           "record_stop": None}
add("rollback_if_pulled_syntax_error", {"git_cmd": GIT, "pre_pull_sha": SHA, "rollback_branch": None, "token": None},
    dict(rb_base, tree_syntax_error=None))  # no broken -> return
add("rollback_if_pulled_syntax_error", {"git_cmd": GIT, "pre_pull_sha": SHA, "rollback_branch": None, "token": None},
    dict(rb_base, tree_syntax_error=[["cli.py", "SyntaxError: bad"]], git_run=[rg(0), rg(0), rg(0)]))
add("rollback_if_pulled_syntax_error", {"git_cmd": GIT, "pre_pull_sha": SHA, "rollback_branch": "feature", "token": None},
    dict(rb_base, tree_syntax_error=[["cli.py", "bad"]], git_run=[rg(0), rg(0), rg(0), rg(0)]))  # detach + parked checkout
add("rollback_if_pulled_syntax_error", {"git_cmd": GIT, "pre_pull_sha": None, "rollback_branch": None, "token": None},
    dict(rb_base, tree_syntax_error=[["cli.py", "bad"]]))  # no pre sha
add("rollback_if_pulled_syntax_error", {"git_cmd": GIT, "pre_pull_sha": SHA, "rollback_branch": None, "token": None},
    dict(rb_base, tree_syntax_error=[["cli.py", "line1\nline2\nline3"]], git_run=[rg(0), rg(0), rg(1, "", "hard fail")],
         settle_failed_tree_move=True))  # rollback failed but settled
add("rollback_if_pulled_syntax_error", {"git_cmd": GIT, "pre_pull_sha": SHA, "rollback_branch": None, "token": None},
    dict(rb_base, tree_syntax_error=rs("RuntimeError", "scan boom")))
add("rollback_if_pulled_syntax_error", {"git_cmd": GIT, "pre_pull_sha": SHA, "rollback_branch": None, "token": None},
    {k: v for k, v in rb_base.items() if k != "tree_syntax_error"})  # fail-closed

# ── roll_back_broken_update (direct) ──
add("roll_back_broken_update", {"git_cmd": GIT, "pre_pull_sha": SHA, "failing_path": "cli.py",
                                "syntax_error": "SyntaxError: invalid", "rollback_branch": None, "token": None},
    dict(rb_base, git_run=[rg(0), rg(0), rg(0)]))
add("roll_back_broken_update", {"git_cmd": GIT, "pre_pull_sha": SHA, "failing_path": "cli.py",
                                "syntax_error": None, "rollback_branch": "HEAD", "token": None},
    dict(rb_base, git_run=[rg(0), rg(0), rg(0)]))  # HEAD -> not parked, detach mode
add("roll_back_broken_update", {"git_cmd": GIT, "pre_pull_sha": SHA, "failing_path": "cli.py",
                                "syntax_error": "e1\ne2\ne3\ne4\ne5\ne6\ne7", "rollback_branch": "feat", "token": None},
    dict(rb_base, git_run=[rg(0), rg(0), rg(0), rg(1)]))  # 6 lines cap + parked checkout fail
add("roll_back_broken_update", {"git_cmd": GIT, "pre_pull_sha": SHA, "failing_path": "cli.py",
                                "syntax_error": "bad", "rollback_branch": None, "token": None},
    dict(rb_base, arm_tree_move=rs("OSError", "marker write fail"), git_run=[rg(0), rg(0), rg(0)]))
add("roll_back_broken_update", {"git_cmd": GIT, "pre_pull_sha": SHA, "failing_path": "cli.py",
                                "syntax_error": "bad", "rollback_branch": None, "token": None},
    dict(rb_base, git_run=[rg(0), rg(0), rg(1, "", "hard reset failed")], marker_is_file=True))
add("roll_back_broken_update", {"git_cmd": GIT, "pre_pull_sha": None, "failing_path": "cli.py",
                                "syntax_error": "bad", "rollback_branch": None, "token": None}, dict(rb_base))
add("roll_back_broken_update", {"git_cmd": GIT, "pre_pull_sha": SHA, "failing_path": "cli.py",
                                "syntax_error": "bad", "rollback_branch": None, "token": None},
    dict(rb_base, **{"checkout_move.enter": rs("RuntimeError", "gate fail")}))  # CM enter raise
add("roll_back_broken_update", {"git_cmd": GIT, "pre_pull_sha": SHA, "failing_path": "cli.py",
                                "syntax_error": "bad", "rollback_branch": None, "token": None},
    dict(rb_base, git_run=[rg(0), rg(0), rg(0)], **{"checkout_move.exit": rs("RuntimeError", "exit fail")}))

# ── move_checkout_to ──
mc_base = {"project_root": ROOT, "park_detached_head": None, "record_stop": None, "git_output_stop_class": None}
add("move_checkout_to", {"git_cmd": GIT, "branch": "main", "merge_ref": "origin/main", "target_sha": SHA, "pre_pull_sha": SHA2},
    dict(mc_base, git_run=[rg(0)]))  # ff-only success
add("move_checkout_to", {"git_cmd": GIT, "branch": "main", "merge_ref": SHA2, "target_sha": SHA, "pre_pull_sha": SHA2},
    dict(mc_base, git_run=[rg(0)]))  # detached checkout (merge_ref != origin/main)
add("move_checkout_to", {"git_cmd": GIT, "branch": "main", "merge_ref": "origin/main", "target_sha": SHA, "pre_pull_sha": SHA2},
    dict(mc_base, git_run=[rg(1), rg(1)], reconcile_diverged_checkout=None,
         time_strftime="t", utc_now_strftime="t", prune_orphan_rescue_refs=None))  # ancestry rc1 -> reconcile
add("move_checkout_to", {"git_cmd": GIT, "branch": "main", "merge_ref": "origin/main", "target_sha": SHA, "pre_pull_sha": SHA2},
    dict(mc_base, git_run=[rg(1, "", "merge err"), rg(0)], git_output_stop_class="lock_conflict"))  # ancestor rc0
add("move_checkout_to", {"git_cmd": GIT, "branch": "main", "merge_ref": "origin/main", "target_sha": SHA, "pre_pull_sha": SHA2},
    dict(mc_base, git_run=[rg(1, "out", "err"), rg(128, "anc out", "anc err")]))  # ancestry other
add("move_checkout_to", {"git_cmd": GIT, "branch": "main", "merge_ref": "origin/main", "target_sha": SHA, "pre_pull_sha": SHA2},
    {k: v for k, v in mc_base.items() if k != "git_run"})  # fail-closed

# ── pull_updates ──
pu_base = {
    "project_root": ROOT, "capture_head_sha": SHA2, "interrupted_pull_marker": ROOT + "/.git/ip",
    "is_object_id": True, "update_movement_baseline": SHA2, "arm_commit_point": None,
    "checkout_move.enter": None, "checkout_move.exit": None, "refuse_unselected_head": None,
    "marker_unlink": None, "verify_head_after_pull": SHA, "rollback_if_pulled_syntax_error": None,
    "tree_syntax_error": None, "record_stop": None, "clear_pending_autostash": None,
    "moves_for": None, "restore_stashed_changes": None, "discard_stashed_changes": None,
    "park_stashed_changes": None, "push_synced_fork": None, "sync_with_upstream_if_needed": True,
}
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": None},
    dict(pu_base, git_run=[rg(0, SHA), rg(0)]))  # rev-parse + merge ff success
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": "stash@{0}"},
    dict(pu_base, git_run=[rg(0, SHA), rg(0)]))  # success + restore stash
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": "stash@{0}", "discard_local_changes": True},
    dict(pu_base, git_run=[rg(0, SHA), rg(0)]))  # discard
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": "stash@{0}", "keep_stash": True},
    dict(pu_base, git_run=[rg(0, SHA), rg(0)]))  # keep
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": None},
    dict(pu_base, git_run=[rg(0, "notasha")], is_object_id=False))  # target unresolved
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": None, "expected_sha": SHA3},
    dict(pu_base, git_run=[rg(0, SHA)]))  # expected mismatch
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": None},
    dict(pu_base, git_run=[rg(0, SHA)], arm_commit_point="refused reason"))  # commit point refused
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": "stash@{0}"},
    dict(pu_base, git_run=[rg(0, SHA), rg(1)], settle_failed_tree_move=False, owe_restore_of=None,
         reconcile_diverged_checkout=None, time_strftime="t", utc_now_strftime="t", prune_orphan_rescue_refs=None))  # move fails -> finally stash notice
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": None, "sync_upstream": True},
    dict(pu_base, git_run=[rg(0, SHA), rg(0)]))  # sync upstream path
add("pull_updates", {"git_cmd": GIT, "branch": "main", "auto_stash_ref": None},
    {k: v for k, v in pu_base.items() if k != "capture_head_sha"})  # fail-closed

# ── switch_branch_at_commit_point ──
sw_base = {"project_root": ROOT, "arm_commit_point": None, "settle_failed_tree_move": False,
           "disarm_commit_obligations": None, "owe_restore_of": None, "landed_off_target": None,
           "interrupted_pull_marker": ROOT + "/.git/ip", "marker_unlink": None}
add("switch_branch_at_commit_point", {"git_cmd": GIT, "branch": "main", "target_ref": "origin/main", "pre": SHA2, "stash": None},
    dict(sw_base, git_run=[rg(0, SHA), rg(0, SHA2), rg(0)]))  # local resolve, remote resolve, checkout ok
add("switch_branch_at_commit_point", {"git_cmd": GIT, "branch": "main", "target_ref": "origin/main", "pre": SHA2, "stash": None},
    dict(sw_base, git_run=[rg(0, ""), rg(0, SHA2), rg(0)]))  # no local -> -B form
add("switch_branch_at_commit_point", {"git_cmd": GIT, "branch": "main", "target_ref": "origin/main", "pre": SHA2, "stash": None},
    dict(sw_base, git_run=[rg(0, SHA), rg(0, SHA2)], arm_commit_point="refused"))  # arm refused
add("switch_branch_at_commit_point", {"git_cmd": GIT, "branch": "main", "target_ref": "origin/main", "pre": SHA2, "stash": None},
    dict(sw_base, git_run=[rg(0, SHA), rg(0, SHA2), rg(0)], landed_off_target=SHA3))  # ref moved
add("switch_branch_at_commit_point", {"git_cmd": GIT, "branch": "main", "target_ref": "origin/main", "pre": SHA2, "stash": None},
    dict(sw_base, git_run=[rg(0, SHA), rg(0, SHA2), rg(1, "", "checkout fail")], settle_failed_tree_move=False))
add("switch_branch_at_commit_point", {"git_cmd": GIT, "branch": "main", "target_ref": "origin/main", "pre": SHA2, "stash": None},
    dict(sw_base, git_run=[rg(0, ""), rg(0, "")]))  # both unresolvable -> -B git fails
add("switch_branch_at_commit_point", {"git_cmd": GIT, "branch": "main", "target_ref": "origin/main", "pre": SHA2, "stash": None},
    {k: v for k, v in sw_base.items() if k != "git_run"})  # fail-closed

# ── refuse_before_commit_point ──
rf_base = {"project_root": ROOT, "preflight_refusal": None, "downgrade_refusal": None,
           "record_stop": None, "resume_windows_gateways_after_update": None}
add("refuse_before_commit_point", {"git_cmd": GIT, "target_ref": "origin/main", "token": None}, dict(rf_base))
add("refuse_before_commit_point", {"git_cmd": GIT, "target_ref": "origin/main", "token": None},
    dict(rf_base, preflight_refusal="preflight reason"))
add("refuse_before_commit_point", {"git_cmd": GIT, "target_ref": "origin/main", "token": None},
    dict(rf_base, downgrade_refusal="downgrade reason"))
add("refuse_before_commit_point", {"git_cmd": GIT, "target_ref": "origin/main", "token": {"resume_needed": True}},
    dict(rf_base, preflight_refusal="reason"))
add("refuse_before_commit_point", {"git_cmd": GIT, "target_ref": "origin/main", "token": None},
    dict(rf_base, preflight_refusal=rs("RuntimeError", "boom")))
add("refuse_before_commit_point", {"git_cmd": GIT, "target_ref": "origin/main", "token": None},
    {k: v for k, v in rf_base.items() if k != "preflight_refusal"})  # fail-closed

# ── exit_after_failed_branch_switch ──
ex_base = {"project_root": ROOT, "interrupted_pull_marker": ROOT + "/.git/ip", "marker_is_file": False,
           "record_stop": None, "clear_pending_autostash": None, "moves_for": None,
           "restore_stashed_changes": None, "missing_branch_stop": "branch_missing"}
add("exit_after_failed_branch_switch", {"git_cmd": GIT, "branch": "main",
                                        "track_result": {"args": ["arm_commit_point"], "returncode": 1, "stdout": "", "stderr": "refused"},
                                        "auto_stash_ref": None, "gw_input_fn": None, "token": None}, dict(ex_base))
add("exit_after_failed_branch_switch", {"git_cmd": GIT, "branch": "main",
                                        "track_result": {"args": ["ref_moved"], "returncode": 1, "stdout": "", "stderr": "moved"},
                                        "auto_stash_ref": None, "gw_input_fn": None, "token": None}, dict(ex_base))
add("exit_after_failed_branch_switch", {"git_cmd": GIT, "branch": "main",
                                        "track_result": {"args": ["checkout"], "returncode": 1, "stdout": "", "stderr": "no branch"},
                                        "auto_stash_ref": None, "gw_input_fn": None, "token": None}, dict(ex_base))
add("exit_after_failed_branch_switch", {"git_cmd": GIT, "branch": "main",
                                        "track_result": {"args": ["checkout"], "returncode": 1, "stdout": "", "stderr": "torn"},
                                        "auto_stash_ref": "stash@{0}", "gw_input_fn": None, "token": None},
    dict(ex_base, marker_is_file=True))  # torn + stash notice
add("exit_after_failed_branch_switch", {"git_cmd": GIT, "branch": "main",
                                        "track_result": {"args": ["checkout"], "returncode": 1, "stdout": "", "stderr": "x"},
                                        "auto_stash_ref": "stash@{0}", "gw_input_fn": {"fn": 1}, "token": None},
    dict(ex_base))  # restore stash before bail
add("exit_after_failed_branch_switch", {"git_cmd": GIT, "branch": "main",
                                        "track_result": {"args": ["checkout"], "returncode": 1, "stdout": "", "stderr": ""},
                                        "auto_stash_ref": None, "gw_input_fn": None, "token": None},
    {k: v for k, v in ex_base.items() if k != "marker_is_file"})  # fail-closed

# ── apply_parked_branch_guard ──
ap_base = {"project_root": ROOT, "assess_parked_branch_switch": [[True, "fully merged"]],
           "print_parked_branch_skip_warning": None, "branch_head_suffix": "",
           "resume_windows_gateways_after_update": None, "record_stop": None,
           "print_parked_branch_kept_notice": None, "updates_config": {}}
add("apply_parked_branch_guard", {"git_cmd": GIT, "branch": "main", "current_branch": "main", "switch_branch": False, "token": None},
    dict(ap_base))  # same branch -> early return
add("apply_parked_branch_guard", {"git_cmd": GIT, "branch": "main", "current_branch": "HEAD", "switch_branch": False, "token": None},
    dict(ap_base))  # HEAD -> early return
add("apply_parked_branch_guard", {"git_cmd": GIT, "branch": "main", "current_branch": "feat", "switch_branch": False, "token": None},
    dict(ap_base))  # fully merged -> switch back
add("apply_parked_branch_guard", {"git_cmd": GIT, "branch": "main", "current_branch": "feat", "switch_branch": False, "token": None},
    dict(ap_base, assess_parked_branch_switch=[[False, "dirty"]]))  # unsafe -> skip warning + exit
add("apply_parked_branch_guard", {"git_cmd": GIT, "branch": "main", "current_branch": "feat", "switch_branch": False, "token": None},
    dict(ap_base, assess_parked_branch_switch=[[True, "unmerged:3"]]))  # kept notice (default switch)
add("apply_parked_branch_guard", {"git_cmd": GIT, "branch": "main", "current_branch": "feat", "switch_branch": False, "token": None},
    dict(ap_base, assess_parked_branch_switch=[[True, "unmerged:3"]],
         updates_config={"parked_branch_strategy": "update_in_place"}, git_run=[rg(0)]))  # in place
add("apply_parked_branch_guard", {"git_cmd": GIT, "branch": "main", "current_branch": "feat", "switch_branch": False, "token": None},
    dict(ap_base, assess_parked_branch_switch=[[True, "unmerged:3"]],
         updates_config={"parked_branch_strategy": "update_in_place"}, git_run=[rg(1)]))  # branch missing
add("apply_parked_branch_guard", {"git_cmd": GIT, "branch": "main", "current_branch": "feat", "switch_branch": True, "token": None},
    dict(ap_base, assess_parked_branch_switch=[[True, "unmerged:3"]],
         updates_config={"parked_branch_strategy": "update_in_place"}))  # switch_branch override
add("apply_parked_branch_guard", {"git_cmd": GIT, "branch": "main", "current_branch": "feat", "switch_branch": False, "token": None},
    {k: v for k, v in ap_base.items() if k != "assess_parked_branch_switch"})  # fail-closed

# ── prepare_checkout_for_update ──
pc_base = {
    "project_root": ROOT, "assess_parked_branch_switch": [[True, "fully merged"]],
    "print_parked_branch_skip_warning": None, "branch_head_suffix": "",
    "resume_windows_gateways_after_update": None, "record_stop": None,
    "print_parked_branch_kept_notice": None, "updates_config": {},
    "stdin_isatty": False, "stdout_isatty": False, "target_would_move_head": False,
    "moves_for": None, "stash_local_changes_if_needed": None, "capture_head_sha": SHA,
    "is_shallow_checkout": False, "git_run": [rg(0, "0")], "park_detached_head": None,
}
add("prepare_checkout_for_update", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                    "is_fork": False, "assume_yes": True, "gateway_mode": False,
                                    "gw_input_fn": None, "switch_branch": False, "target_ref": None, "token": None},
    dict(pc_base))  # up to date (count 0)
add("prepare_checkout_for_update", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                    "is_fork": False, "assume_yes": True, "gateway_mode": False,
                                    "gw_input_fn": None, "switch_branch": False, "target_ref": None, "token": None},
    dict(pc_base, git_run=[rg(0, "5")]))  # 5 commits
add("prepare_checkout_for_update", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                    "is_fork": False, "assume_yes": True, "gateway_mode": False,
                                    "gw_input_fn": None, "switch_branch": False, "target_ref": SHA2, "token": None},
    dict(pc_base))  # release tag
add("prepare_checkout_for_update", {"git_cmd": GIT, "branch": "main", "current_branch": "feat",
                                    "is_fork": False, "assume_yes": True, "gateway_mode": False,
                                    "gw_input_fn": None, "switch_branch": False, "target_ref": None, "token": None},
    dict(pc_base, pause_at_commit_point=None, resolved_commit=[SHA, SHA2],
         **{"checkout_move.enter": None, "checkout_move.exit": None},
         switch_branch_at_commit_point=None, arm_commit_point=None,
         settle_failed_tree_move=False, disarm_commit_obligations=None, owe_restore_of=None,
         landed_off_target=None, interrupted_pull_marker=ROOT + "/.git/ip", marker_unlink=None,
         git_run=[rg(0, SHA), rg(0, SHA2), rg(0), rg(0, "3")]))  # parked switch
add("prepare_checkout_for_update", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                    "is_fork": False, "assume_yes": True, "gateway_mode": False,
                                    "gw_input_fn": None, "switch_branch": False, "target_ref": None, "token": None},
    dict(pc_base, git_run=[rg(0, "5")], is_shallow_checkout=True, tip_shas=[[SHA, SHA2]],
         github_compare_behind=7))  # shallow compare
add("prepare_checkout_for_update", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                    "is_fork": True, "assume_yes": True, "gateway_mode": False,
                                    "gw_input_fn": None, "switch_branch": False, "target_ref": None, "token": None},
    dict(pc_base, git_run=[rg(0, "0")], sync_with_upstream_if_needed=True,
         capture_head_sha=[SHA, SHA, SHA2], count_commits_between=4))  # fork sync moves head
add("prepare_checkout_for_update", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                    "is_fork": False, "assume_yes": False, "gateway_mode": True,
                                    "gw_input_fn": {"fn": 1}, "switch_branch": False, "target_ref": None, "token": None},
    dict(pc_base, stash_local_changes_if_needed="stash@{0}", git_run=[rg(0, "0")]))  # gateway prompt
add("prepare_checkout_for_update", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                    "is_fork": False, "assume_yes": True, "gateway_mode": False,
                                    "gw_input_fn": None, "switch_branch": False, "target_ref": None, "token": None},
    {k: v for k, v in pc_base.items() if k != "stash_local_changes_if_needed"})  # fail-closed

# ── resolve_update_options ──
ro_base = {"checkout_version": "1.0", "stdin_isatty": False, "stdout_isatty": False, "updates_config": {}}
add("resolve_update_options", {"args": {"yes": True}, "gateway_mode": False}, dict(ro_base))
add("resolve_update_options", {"args": {}, "gateway_mode": True}, dict(ro_base))
add("resolve_update_options", {"args": {"keep_stash": True, "switch_branch": True, "no_gateway_restart": True, "force": True},
                               "gateway_mode": False}, dict(ro_base, updates_config={"non_interactive_local_changes": "discard"}))
add("resolve_update_options", {"args": None, "gateway_mode": False}, dict(ro_base))
add("resolve_update_options", {"args": {"yes": False}, "gateway_mode": False},
    dict(ro_base, stdin_isatty=True, stdout_isatty=True))  # interactive -> no config read
add("resolve_update_options", {"args": {}, "gateway_mode": False},
    dict(ro_base, updates_config={"non_interactive_local_changes": "DISCARD"}))  # lower()
add("resolve_update_options", {"args": {}, "gateway_mode": False},
    {k: v for k, v in ro_base.items() if k != "checkout_version"})  # fail-closed

# ── begin_update_receipt_and_plan ──
bp_base = {"begin_update_receipt": None, "update_marker_path": ROOT + "/.git/update-marker",
           "marker_read_text": "999\nrest", "getpid": 100, "record_fact": None,
           "collect_runtime_inventory": {"runtimes": [{"profile": "a"}, {"profile": "b"}]},
           "record_plan_in_receipt": None, "record_stage": None}
add("begin_update_receipt_and_plan", {}, dict(bp_base))
add("begin_update_receipt_and_plan", {}, dict(bp_base, collect_runtime_inventory={"runtimes": []}))
add("begin_update_receipt_and_plan", {}, dict(bp_base, collect_runtime_inventory=None))  # plan failed
add("begin_update_receipt_and_plan", {}, dict(bp_base, marker_read_text="100\n"))  # pid == getpid -> no fact
add("begin_update_receipt_and_plan", {}, dict(bp_base, marker_read_text="abc\n"))  # not digit
add("begin_update_receipt_and_plan", {}, dict(bp_base, begin_update_receipt=rs("RuntimeError", "receipt boom")))
add("begin_update_receipt_and_plan", {},
    {k: v for k, v in bp_base.items() if k != "collect_runtime_inventory"})  # fail-closed (swallowed)

# ── record_update_initiator ──
ri_base = {"update_marker_path": ROOT + "/.git/update-marker", "marker_read_text": "999\nrest",
           "getpid": 100, "record_fact": None}
add("record_update_initiator", {}, dict(ri_base))
add("record_update_initiator", {}, dict(ri_base, marker_read_text="100\n"))  # same pid
add("record_update_initiator", {}, dict(ri_base, marker_read_text="abc"))  # not digit
add("record_update_initiator", {}, dict(ri_base, marker_read_text="  42  \n"))  # strip + digit
add("record_update_initiator", {}, dict(ri_base, marker_read_text=""))  # empty
add("record_update_initiator", {}, dict(ri_base, update_marker_path=rs("OSError", "no marker")))  # swallowed
add("record_update_initiator", {}, {})  # fail-closed (swallowed)

# ── prepare_git_command ──
pg_base = {"project_root": ROOT, "path_exists": True, "expose_pm_git": None, "base_git_cmd": GIT,
           "map_ssl_cert_file_for_git": None, "ensure_non_trampoline_git": GIT,
           "enclosing_repo_of_checkout": None, "damaged_checkout_message": "damaged",
           "discard_lockfile_churn": None, "normalize_managed_eol": None,
           "get_origin_url": "https://github.com/x/y", "is_fork": False, "record_stop": None}
add("prepare_git_command", {"checkout_move": None, "platform": "linux"}, dict(pg_base))
add("prepare_git_command", {"checkout_move": None, "platform": "linux"}, dict(pg_base, is_fork=True))
add("prepare_git_command", {"checkout_move": None, "platform": "linux"}, dict(pg_base, path_exists=False))  # zip, non-win -> exit
add("prepare_git_command", {"checkout_move": None, "platform": "win32"}, dict(pg_base, path_exists=False))  # zip win ok
add("prepare_git_command", {"checkout_move": None, "platform": "win32"}, dict(pg_base, git_run=[rg(0)]))  # win config
add("prepare_git_command", {"checkout_move": None, "platform": "linux"},
    dict(pg_base, enclosing_repo_of_checkout="/enclosing"))  # enclosing repo refusal
add("prepare_git_command", {"checkout_move": {"cm": 1}, "platform": "linux"}, dict(pg_base))
add("prepare_git_command", {"checkout_move": None, "platform": "linux"},
    {k: v for k, v in pg_base.items() if k != "base_git_cmd"})  # fail-closed

# ── handle_update_called_process_error ──
E = {"returncode": 1, "cmd": ["git", "fetch"], "output": None, "stderr": "fetch err"}
he_base = {"unrestored_autostash_notice": None, "format_update_failure_stage": "fetch",
           "should_zip_fallback_on_update_error": False, "called_process_error_is_python_dep_install": False,
           "print_called_process_error_tail": None, "git_error_stop_class": "fetch_failed",
           "record_stop": None, "finalize_receipt": None, "is_windows": False, "update_via_zip": None}
add("handle_update_called_process_error", {"e": E, "args": {"yes": True}, "gateway_mode": False,
                                           "had_desktop_app_before_update": False, "target_sha": None,
                                           "target_repository": None, "completion_request": None}, dict(he_base))
add("handle_update_called_process_error", {"e": E, "args": {}, "gateway_mode": False,
                                           "had_desktop_app_before_update": False, "target_sha": None,
                                           "target_repository": None, "completion_request": None},
    dict(he_base, should_zip_fallback_on_update_error=True))  # zip fallback
add("handle_update_called_process_error", {"e": E, "args": {}, "gateway_mode": False,
                                           "had_desktop_app_before_update": True, "target_sha": SHA,
                                           "target_repository": "https://x", "completion_request": {"a": 1}},
    dict(he_base, should_zip_fallback_on_update_error=True))  # zip with target_repository
add("handle_update_called_process_error", {"e": E, "args": {}, "gateway_mode": False,
                                           "had_desktop_app_before_update": False, "target_sha": None,
                                           "target_repository": None, "completion_request": None},
    dict(he_base, called_process_error_is_python_dep_install=True))  # dep install
add("handle_update_called_process_error", {"e": E, "args": {}, "gateway_mode": False,
                                           "had_desktop_app_before_update": False, "target_sha": None,
                                           "target_repository": None, "completion_request": None},
    dict(he_base, called_process_error_is_python_dep_install=True, is_windows=True))  # dep + windows
add("handle_update_called_process_error", {"e": E, "args": {}, "gateway_mode": False,
                                           "had_desktop_app_before_update": False, "target_sha": None,
                                           "target_repository": None, "completion_request": None},
    dict(he_base, unrestored_autostash_notice="stash left", git_error_stop_class=None))
add("handle_update_called_process_error", {"e": E, "args": {}, "gateway_mode": False,
                                           "had_desktop_app_before_update": False, "target_sha": None,
                                           "target_repository": None, "completion_request": None},
    {k: v for k, v in he_base.items() if k != "format_update_failure_stage"})  # fail-closed

# ── finish_already_up_to_date ──
PLAN = {"auto_stash_ref": None, "prompt_for_restore": False, "parked_branch_switched": False,
        "switch_block_reason": None, "upstream_checked": True, "in_place_update": False}
fu_base = dict(cs_base, capture_head_sha=SHA, moves_for=None, restore_stashed_changes=None)
add("finish_already_up_to_date", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                  "plan": dict(PLAN), "gw_input_fn": None,
                                  "completion_request": dict(REQ), "token": None, "held_message": None}, dict(fu_base))
add("finish_already_up_to_date", {"git_cmd": GIT, "branch": "main", "current_branch": "feat",
                                  "plan": dict(PLAN, parked_branch_switched=True, switch_block_reason="unmerged:2"),
                                  "gw_input_fn": None, "completion_request": dict(REQ), "token": None, "held_message": None},
    dict(fu_base))
add("finish_already_up_to_date", {"git_cmd": GIT, "branch": "main", "current_branch": "feat",
                                  "plan": dict(PLAN, parked_branch_switched=True, switch_block_reason="fully merged"),
                                  "gw_input_fn": None, "completion_request": dict(REQ), "token": None, "held_message": None},
    dict(fu_base))
add("finish_already_up_to_date", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                  "plan": dict(PLAN, auto_stash_ref="stash@{0}", prompt_for_restore=True),
                                  "gw_input_fn": {"fn": 1}, "completion_request": dict(REQ), "token": None, "held_message": None},
    dict(fu_base))
add("finish_already_up_to_date", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                  "plan": dict(PLAN, upstream_checked=False), "gw_input_fn": None,
                                  "completion_request": dict(REQ), "token": None, "held_message": "held build"},
    dict(fu_base))
add("finish_already_up_to_date", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                  "plan": dict(PLAN), "gw_input_fn": None,
                                  "completion_request": None, "token": None, "held_message": None},
    {"unrestored_autostash_notice": None, "stop_for_relaunch": rs("SystemExit", "0")})
add("finish_already_up_to_date", {"git_cmd": GIT, "branch": "main", "current_branch": "main",
                                  "plan": dict(PLAN), "gw_input_fn": None,
                                  "completion_request": dict(REQ), "token": None, "held_message": None},
    {k: v for k, v in fu_base.items() if k != "capture_head_sha"})  # fail-closed

# ── apply_pulled_update ──
au_base = dict(cs_base, verify_head_after_pull=SHA, capture_head_sha=SHA,
               resume_windows_gateways_after_update=None, record_stop=None)
add("apply_pulled_update", {"git_cmd": GIT, "branch": "main", "movement_baseline": SHA2,
                            "plan": dict(PLAN), "token": None, "completion_request": dict(REQ)}, dict(au_base))
add("apply_pulled_update", {"git_cmd": GIT, "branch": "main", "movement_baseline": SHA2,
                            "plan": dict(PLAN), "token": None, "completion_request": dict(REQ, expected_sha=SHA)},
    dict(au_base))  # pinned matches
add("apply_pulled_update", {"git_cmd": GIT, "branch": "main", "movement_baseline": SHA2,
                            "plan": dict(PLAN), "token": None, "completion_request": dict(REQ, expected_sha=SHA3)},
    dict(au_base))  # pinned mismatch -> exit
add("apply_pulled_update", {"git_cmd": GIT, "branch": "main", "movement_baseline": SHA2,
                            "plan": dict(PLAN, in_place_update=True), "token": None, "completion_request": None},
    {"unrestored_autostash_notice": None, "stop_for_relaunch": rs("SystemExit", "0"),
     "verify_head_after_pull": SHA})
add("apply_pulled_update", {"git_cmd": GIT, "branch": "main", "movement_baseline": SHA2,
                            "plan": dict(PLAN), "token": None, "completion_request": dict(REQ)},
    dict(au_base, verify_head_after_pull=rs("SystemExit", "1")))  # verify exits
add("apply_pulled_update", {"git_cmd": GIT, "branch": "main", "movement_baseline": SHA2,
                            "plan": dict(PLAN), "token": None, "completion_request": dict(REQ)},
    {k: v for k, v in au_base.items() if k != "verify_head_after_pull"})  # fail-closed


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-36s %d" % (k, counts[k]))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
