#!/usr/bin/env python3
"""Generate tests/fixtures/web_git_flow.jsonl for the web_git FLOW port.

Each case is {"func": <live name>, "in": {...inputs..., "io": {seam: script}}}.
Script semantics (shared with the C harness dispatcher and the oracle Ctx):
  - a plain JSON value is returned verbatim on EVERY call (constant);
  - a JSON ARRAY is a QUEUE consumed one element per call (last repeats? no --
    exhausted => fail-closed RuntimeError); list-valued RESULTS must be wrapped
    one level deeper or expressed as {"__script__": true, "ret": <value>};
  - {"__script__": true, "ret": v} returns v; {"__script__": true,
    "raise": {"type": T, "message": m}} raises T(m) (message == str(exc));
  - a missing/exhausted script is a fail-closed RuntimeError on both sides.

Seam result shapes: git/git_write/gh -> [a,b,c]; walk_entries ->
[[tag,xy,path],...]; git_out/untracked_insertions/sanitize_branch/
tracking_refspec/path.resolve -> scalar; path.is_dir/path.is_file -> bool.

NUL-record convention: porcelain -z payloads encode NUL as \x01 (libjson
strings are NUL-terminated); the oracle decodes \x01 -> \x00 on its side.
ASCII domain throughout (int()/str()/json.dumps positions).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "web_git_flow.jsonl")


def const(ret):
    return {"__script__": True, "ret": ret}


def raise_(typ, msg):
    return {"__script__": True, "raise": {"type": typ, "message": msg}}


CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


# -- _git_line -------------------------------------------------------------
add("_git_line", cwd="/r", args=["rev-parse", "HEAD"], io={"git_out": "  main  "})
add("_git_line", cwd="/r", args=["x"], io={"git_out": ""})
add("_git_line", cwd="/r", args=["x"], run="write", io={"git_out": "x"})
add("_git_line", cwd="/r", args=["x"], io={"git_out": raise_("RuntimeError", "boom")})
add("_git_line", cwd="/r", args=["x"], io={"git_out": None})
add("_git_line", cwd="/r", args=["x"], io={})
add("_git_line", cwd="/r", args=["x"], io={"git_out": raise_("SystemExit", "bye")})

# -- _is_dir ---------------------------------------------------------------
add("_is_dir", cwd="/r", io={"path.is_dir": True})
add("_is_dir", cwd="/r", io={"path.is_dir": False})
add("_is_dir", cwd="/r", io={"path.is_dir": raise_("OSError", "x")})
add("_is_dir", cwd="/r", io={"path.is_dir": raise_("RuntimeError", "x")})
add("_is_dir", cwd=None, io={})
add("_is_dir", cwd="/r", io={"path.is_dir": raise_("SystemExit", "x")})
add("_is_dir", cwd="/r", io={})

# -- _status_z -------------------------------------------------------------
add("_status_z", cwd="/r", io={"git": const([0, "raw", ""])})
add("_status_z", cwd="/r", extra=["--branch"], io={"git": const([0, "# branch.head main\x01", ""])})
add("_status_z", cwd="/r", run="write", io={"git_write": const([1, "", "err"])})
add("_status_z", cwd="/r", io={"git": raise_("RuntimeError", "z")})
add("_status_z", cwd="/r", io={})
add("_status_z", cwd="/r", io={"git": raise_("SystemExit", "z")})

# -- _origin_head ----------------------------------------------------------
add("_origin_head", cwd="/r", io={"git_out": "main"})
add("_origin_head", cwd="/r", io={"git_out": ""})
add("_origin_head", cwd="/r", run="write", io={"git_out": "origin/main"})
add("_origin_head", cwd="/r", io={"git_out": raise_("RuntimeError", "oh")})
add("_origin_head", cwd="/r", io={})

# -- _origin_head_abbrev ---------------------------------------------------
add("_origin_head_abbrev", cwd="/r", io={"git_out": "origin/main"})
add("_origin_head_abbrev", cwd="/r", io={"git_out": ""})
add("_origin_head_abbrev", cwd="/r", io={"git_out": "  origin/master  "})
add("_origin_head_abbrev", cwd="/r", io={"git_out": raise_("RuntimeError", "a")})
add("_origin_head_abbrev", cwd="/r", io={})

# -- _ref_exists -----------------------------------------------------------
add("_ref_exists", cwd="/r", ref="HEAD", io={"git": const([0, "", ""])})
add("_ref_exists", cwd="/r", ref="HEAD", io={"git": const([1, "", ""])})
add("_ref_exists", cwd="/r", ref="HEAD", run="write", io={"git_write": const([0, "", ""])})
add("_ref_exists", cwd="/r", ref="HEAD", io={"git": raise_("RuntimeError", "re")})
add("_ref_exists", cwd="/r", ref="HEAD", io={})

# -- _entry_staged ---------------------------------------------------------
add("_entry_staged", tag="1", xy="M.")
add("_entry_staged", tag="1", xy=".M")
add("_entry_staged", tag="?", xy="??")
add("_entry_staged", tag="2", xy="R.")
add("_entry_staged", tag="1", xy="")
add("_entry_staged", tag="u", xy="UU")

# -- _classify -------------------------------------------------------------
add("_classify", tag="1", xy="M.", path="a")
add("_classify", tag="1", xy=".M", path="a")
add("_classify", tag="?", xy="??", path="n")
add("_classify", tag="u", xy="UU", path="c")
add("_classify", tag="2", xy="RM", path="r")
add("_classify", tag="1", xy="", path="a")

# -- _status_letter --------------------------------------------------------
add("_status_letter", tag="?", xy="??")
add("_status_letter", tag="u", xy="UU")
add("_status_letter", tag="1", xy="M.")
add("_status_letter", tag="1", xy=".D")
add("_status_letter", tag="1", xy="..")
add("_status_letter", tag="1", xy="")
add("_status_letter", tag="1", xy="A.")

# -- _review_result --------------------------------------------------------
add("_review_result", cwd="/r", base="x",
    files=[{"path": "b", "status": "M", "added": 1, "removed": 0}], io={})
add("_review_result", cwd="/r", base=None,
    files=[{"path": "a", "status": "?", "added": 0, "removed": 0}],
    io={"untracked_insertions": 5})
add("_review_result", cwd="/r", base="b",
    files=[{"path": "z", "status": "M", "added": 0, "removed": 0},
           {"path": "a", "status": "M", "added": 0, "removed": 0}], io={})
add("_review_result", cwd="/r", base=None,
    files=[{"path": "n", "status": "?", "added": 0, "removed": 0}],
    io={"untracked_insertions": raise_("RuntimeError", "ui")})
add("_review_result", cwd="/r", base=None, files=[], io={})
add("_review_result", cwd="/r", base=None,
    files=[{"path": "n", "status": "?", "added": 3, "removed": 0}], io={})

# -- _all_add_diff ---------------------------------------------------------
add("_all_add_diff", cwd="/r", file_path="f",
    io={"path.is_file": True, "git": const([1, "difftext", ""])})
add("_all_add_diff", cwd="/r", file_path="f", io={"path.is_file": False})
add("_all_add_diff", cwd="/r", file_path="f", io={"path.is_file": raise_("RuntimeError", "f")})
add("_all_add_diff", cwd=None, file_path="f", io={})
add("_all_add_diff", cwd="/r", file_path="f",
    io={"path.is_file": True, "git": raise_("SystemExit", "g")})
add("_all_add_diff", cwd="/r", file_path="f", io={})

# -- _single_file_diff -----------------------------------------------------
add("_single_file_diff", cwd="/r", args=[], file_path="f", io={"git_out": ""})
add("_single_file_diff", cwd="/r", args=[], file_path="f",
    io={"git_out": ["f\x01", "/r", "DIFF"], "path.resolve": ["/r/f", "/r/f"]})
add("_single_file_diff", cwd="/r", args=["HEAD"], file_path="f",
    io={"git_out": ["f\x01g\x01", "/r"], "path.resolve": ["/r/f", "/r/f", "/r/g"]})
add("_single_file_diff", cwd="/r", args=[], file_path="f",
    io={"git_out": ["f\x01", raise_("RuntimeError", "tl")]})
add("_single_file_diff", cwd="/r", args=[], file_path="f", io={})
add("_single_file_diff", cwd="/r", args=[], file_path="f",
    io={"git_out": ["f\x01", "/r"], "path.resolve": [raise_("SystemExit", "x")]})

# -- file_diff_vs_head -----------------------------------------------------
add("file_diff_vs_head", cwd="/r", file_path="f", io={"path.is_dir": False})
add("file_diff_vs_head", cwd="/r", file_path="f",
    io={"path.is_dir": True, "git_out": ["", "?? f"], "path.is_file": True,
        "git": const([1, "ALLADD", ""])})
add("file_diff_vs_head", cwd="/r", file_path="f",
    io={"path.is_dir": True, "git_out": ["", "M f"]})
add("file_diff_vs_head", cwd="/r", file_path="f",
    io={"path.is_dir": True, "git_out": ["f\x01", "/r", "HD"], "path.resolve": ["/r/f", "/r/f"]})
add("file_diff_vs_head", cwd="/r", file_path="f", io={})
add("file_diff_vs_head", cwd="/r", file_path="f",
    io={"path.is_dir": True, "git_out": ["", raise_("RuntimeError", "st")]})

# -- review_stage ----------------------------------------------------------
add("review_stage", cwd="/r", file_path="f", io={"git_ok": None})
add("review_stage", cwd="/r", file_path=None, io={"git_ok": None})
add("review_stage", cwd="/r", file_path="", io={"git_ok": None})
add("review_stage", cwd="/r", file_path="f", io={"git_ok": raise_("RuntimeError", "err")})
add("review_stage", cwd="/r", file_path="f", io={})
add("review_stage", cwd="/r", file_path="f", io={"git_ok": raise_("SystemExit", "s")})

# -- review_unstage --------------------------------------------------------
add("review_unstage", cwd="/r", file_path="f", io={"git_ok": None})
add("review_unstage", cwd="/r", file_path=None, io={"git_ok": None})
add("review_unstage", cwd="/r", file_path="f", io={"git_ok": raise_("RuntimeError", "u")})
add("review_unstage", cwd="/r", file_path="f", io={})
add("review_unstage", cwd="/r", file_path="x", io={"git_ok": None})

# -- review_revert ---------------------------------------------------------
add("review_revert", cwd="/r", file_path="f", io={"git_write": const([0, "", ""])})
add("review_revert", cwd="/r", file_path=None, io={"git_write": const([0, "", ""])})
add("review_revert", cwd="/r", file_path="f",
    io={"git_write": [const([0, "", ""]), raise_("RuntimeError", "clean fail")]})
add("review_revert", cwd="/r", file_path="f", io={"git_write": raise_("RuntimeError", "co")})
add("review_revert", cwd="/r", file_path="f", io={})
add("review_revert", cwd="/r", file_path="", io={"git_write": const([0, "", ""])})

# -- review_rev_parse ------------------------------------------------------
add("review_rev_parse", cwd="/r", ref="HEAD", io={"git_out": "abc123"})
add("review_rev_parse", cwd="/r", ref=None, io={"git_out": "abc"})
add("review_rev_parse", cwd="/r", ref="x", io={"git_out": ""})
add("review_rev_parse", cwd="/r", ref="x", io={"git_out": "   "})
add("review_rev_parse", cwd="/r", ref="x", io={"git_out": raise_("RuntimeError", "rp")})
add("review_rev_parse", cwd="/r", ref="x", io={})

# -- _has_staged -----------------------------------------------------------
add("_has_staged", raw="r", io={"walk_entries": const([["1", "M.", "a"]])})
add("_has_staged", raw="r", io={"walk_entries": const([["1", ".M", "a"]])})
add("_has_staged", raw="r", io={"walk_entries": const([])})
add("_has_staged", raw="r", io={"walk_entries": const([["?", "??", "n"], ["2", "R.", "r"]])})
add("_has_staged", raw="r", io={"walk_entries": raise_("RuntimeError", "we")})
add("_has_staged", raw="r", io={})
add("_has_staged", raw="r", io={"walk_entries": const([["1", "", "a"]])})

# -- _review_push ----------------------------------------------------------
add("_review_push", cwd="/r", io={"git_out": "origin/main", "git_ok": None})
add("_review_push", cwd="/r", io={"git_out": ["", "feature"], "git_ok": None})
add("_review_push", cwd="/r", io={"git_out": ["", "HEAD"]})
add("_review_push", cwd="/r", io={"git_out": ["", ""]})
add("_review_push", cwd="/r", io={"git_out": raise_("RuntimeError", "rp")})
add("_review_push", cwd="/r", io={"git_out": "origin/main", "git_ok": raise_("RuntimeError", "push fail")})
add("_review_push", cwd="/r", io={})

# -- review_push -----------------------------------------------------------
add("review_push", cwd="/r", io={"git_out": "origin/main", "git_ok": None})
add("review_push", cwd="/r", io={"git_out": ["", "feature"], "git_ok": None})
add("review_push", cwd="/r", io={"git_out": raise_("RuntimeError", "rp")})
add("review_push", cwd="/r", io={})
add("review_push", cwd="/r", io={"git_out": ["", "HEAD"]})

# -- _gh_json --------------------------------------------------------------
add("_gh_json", cwd="/r", args=["x"], io={"gh": const([True, '{"a":1}', ""])})
add("_gh_json", cwd="/r", args=["x"], io={"gh": const([False, "", ""])})
add("_gh_json", cwd="/r", args=["x"], io={"gh": const([True, "not json", ""])})
add("_gh_json", cwd="/r", args=["x"], io={"gh": const([True, "null", ""])})
add("_gh_json", cwd="/r", args=["x"], io={"gh": raise_("RuntimeError", "gj")})
add("_gh_json", cwd="/r", args=["x"], io={"gh": const([True, None, ""])})
add("_gh_json", cwd="/r", args=["x"], io={})
add("_gh_json", cwd="/r", args=["x"], io={"gh": const([True, "5", ""])})

# -- review_ship_info ------------------------------------------------------
add("review_ship_info", cwd="/r", io={"path.is_dir": False})
add("review_ship_info", cwd="/r", io={"path.is_dir": True, "gh": const([False, "", ""])})
add("review_ship_info", cwd="/r",
    io={"path.is_dir": True, "gh": [const([True, "", ""]),
        const([True, '{"url":"u","state":"OPEN","number":5}', ""])]})
add("review_ship_info", cwd="/r",
    io={"path.is_dir": True, "gh": [const([True, "", ""]), const([True, "null", ""])]})
add("review_ship_info", cwd="/r",
    io={"path.is_dir": True, "gh": [const([True, "", ""]), const([True, '{"state":"OPEN"}', ""])]})
add("review_ship_info", cwd="/r", io={})
add("review_ship_info", cwd="/r", io={"path.is_dir": True, "gh": raise_("SystemExit", "s")})

# -- _pr_query -------------------------------------------------------------
add("_pr_query", owner="o", name="n", branches=["main"], numbers=[])
add("_pr_query", owner="o", name="n", branches=[], numbers=[5])
add("_pr_query", owner="o", name="n", branches=["a", "b"], numbers=[1, 2])
add("_pr_query", owner="o", name="n", branches=[], numbers=[])
add("_pr_query", owner='o"q', name="n\\x", branches=["feat/x"], numbers=[])
add("_pr_query", owner="o", name="n", branches=["main"], numbers=[7])

# -- _pr_payload -----------------------------------------------------------
add("_pr_payload", pr={"headRefName": "main", "isDraft": False, "number": 5,
                       "state": "OPEN", "title": "T", "url": "u"})
add("_pr_payload", pr={})
add("_pr_payload", pr={"number": "7", "state": "Closed", "isDraft": True})
add("_pr_payload", pr={"number": "abc"})
add("_pr_payload", pr={"headRefName": "x", "number": 0, "state": "", "title": "", "url": ""})
add("_pr_payload", pr=None)
add("_pr_payload", pr={"number": 3.0})

# -- _own_pr ---------------------------------------------------------------
add("_own_pr", key="n0", field={"number": 9})
add("_own_pr", key="b0", field={"nodes": [{"isCrossRepository": False, "headRefName": "main"}]})
add("_own_pr", key="b0", field={"nodes": [{"isCrossRepository": True}, {"isCrossRepository": False, "number": 2}]})
add("_own_pr", key="b0", field={"nodes": []})
add("_own_pr", key="b0", field={})
add("_own_pr", key="b0", field={"nodes": [None, {"isCrossRepository": False}]})
add("_own_pr", key="b0", field=None)

# -- review_create_pr ------------------------------------------------------
add("review_create_pr", cwd="/r",
    io={"git_out": "origin/main", "git_ok": None, "gh": const([True, "https://pr\n", ""])})
add("review_create_pr", cwd="/r",
    io={"git_out": "origin/main", "git_ok": raise_("RuntimeError", "push"), "gh": const([True, "url", ""])})
add("review_create_pr", cwd="/r",
    io={"git_out": "origin/main", "git_ok": None, "gh": const([False, "", "no commits"])})
add("review_create_pr", cwd="/r",
    io={"git_out": "origin/main", "git_ok": None, "gh": const([False, "", ""])})
add("review_create_pr", cwd="/r",
    io={"git_out": "origin/main", "git_ok": None, "gh": const([False, "", "x" * 500])})
add("review_create_pr", cwd="/r", io={"git_out": raise_("SystemExit", "s")})
add("review_create_pr", cwd="/r",
    io={"git_out": "origin/main", "git_ok": None, "gh": const([True, "  \n\nurl2\n  ", ""])})
add("review_create_pr", cwd="/r", io={})

# -- worktree_list ---------------------------------------------------------
add("worktree_list", cwd="/r",
    io={"git_out": "worktree /r\nbranch refs/heads/main\n\nworktree /r/w\nbranch refs/heads/feat\ndetached\n"})
add("worktree_list", cwd="/r", io={"git_out": ""})
add("worktree_list", cwd="/r", io={"git_out": "worktree /r\nlocked reason\n"})
add("worktree_list", cwd="/r", run="write", io={"git_out": "worktree /r\n"})
add("worktree_list", cwd="/r", io={"git_out": raise_("RuntimeError", "wl")})
add("worktree_list", cwd="/r", io={})
add("worktree_list", cwd="/r", io={"git_out": "bare\nworktree /r\n"})

# -- _main_root ------------------------------------------------------------
add("_main_root", cwd="/r", io={"git_out": "worktree /r\nbranch refs/heads/main\n"})
add("_main_root", cwd="/x", io={"git_out": ""})
add("_main_root", cwd="/r", io={"git_out": raise_("RuntimeError", "mr")})
add("_main_root", cwd="/r", io={})
add("_main_root", cwd="/r", io={"git_out": "worktree /a\nworktree /b\n"})

# -- _fetch_tracking_ref ---------------------------------------------------
add("_fetch_tracking_ref", root="/r", remote="origin", branch="feat",
    io={"tracking_refspec": "+refs/heads/feat:refs/remotes/origin/feat", "git_write": const([0, "", ""])})
add("_fetch_tracking_ref", root="/r", remote="origin", branch="feat",
    io={"tracking_refspec": "rs", "git_write": const([1, "", ""])})
add("_fetch_tracking_ref", root="/r", remote="origin", branch="feat",
    io={"tracking_refspec": raise_("RuntimeError", "ts")})
add("_fetch_tracking_ref", root="/r", remote="origin", branch="feat", io={})
add("_fetch_tracking_ref", root="/r", remote="origin", branch="feat",
    io={"tracking_refspec": "rs", "git_write": raise_("SystemExit", "g")})

# -- _slugify --------------------------------------------------------------
add("_slugify", name="My Feature")
add("_slugify", name="  Hello World  ")
add("_slugify", name="")
add("_slugify", name=None)
add("_slugify", name="!!!")
add("_slugify", name="a" * 50)
add("_slugify", name="Foo/Bar_Baz")
add("_slugify", name="ABC123")

# -- _default_branch -------------------------------------------------------
add("_default_branch", cwd="/r", io={"git_out": "main"})
add("_default_branch", cwd="/r", io={"git_out": "origin/develop"})
add("_default_branch", cwd="/r", io={"git_out": ["", "trunk"]})
add("_default_branch", cwd="/r", io={"git_out": ["", "", "hash"]})
add("_default_branch", cwd="/r", io={"git_out": ["", "", "", "hash"]})
add("_default_branch", cwd="/r", io={"git_out": ["", "", "", ""]})
add("_default_branch", cwd="/r", io={"git_out": raise_("RuntimeError", "db")})
add("_default_branch", cwd="/r", io={})

# -- _ensure_repo ----------------------------------------------------------
add("_ensure_repo", cwd="/r", io={"git_out": "true", "git_write": const([0, "", ""])})
add("_ensure_repo", cwd="/r", io={"git_out": "false", "git_ok": None})
add("_ensure_repo", cwd="/r", io={"git_out": "true", "git_write": const([1, "", ""]), "git_ok": None})
add("_ensure_repo", cwd="/r", io={"git_out": raise_("RuntimeError", "er")})
add("_ensure_repo", cwd="/r", io={"git_out": "false", "git_ok": raise_("RuntimeError", "init fail")})
add("_ensure_repo", cwd="/r", io={})

# -- _remote_of_ref --------------------------------------------------------
add("_remote_of_ref", cwd="/r", name="origin/main", io={"git_write": const([0, "", ""])})
add("_remote_of_ref", cwd="/r", name="main", io={})
add("_remote_of_ref", cwd="/r", name="origin/main", io={"git_write": const([1, "", ""])})
add("_remote_of_ref", cwd="/r", name="up/feat/x", io={"git_write": const([0, "", ""])})
add("_remote_of_ref", cwd="/r", name="origin/main", io={"git_write": raise_("RuntimeError", "rr")})
add("_remote_of_ref", cwd="/r", name="origin/main", io={})

# -- worktree_remove -------------------------------------------------------
add("worktree_remove", cwd="/r", worktree_path="/r/w", force=False,
    io={"git_out": "worktree /r\n", "git_ok": None})
add("worktree_remove", cwd="/r", worktree_path="/r/w", force=True,
    io={"git_out": "worktree /r\n", "git_ok": None})
add("worktree_remove", cwd="/r", worktree_path="/r/w", force=False,
    io={"git_out": "", "git_ok": None})
add("worktree_remove", cwd="/r", worktree_path="/r/w", force=False,
    io={"git_out": "worktree /r\n", "git_ok": raise_("RuntimeError", "wr")})
add("worktree_remove", cwd="/r", worktree_path="/r/w", force=False, io={})
add("worktree_remove", cwd="/r", worktree_path="/r/w", force=False,
    io={"git_out": raise_("SystemExit", "s")})

# -- _ref_names ------------------------------------------------------------
add("_ref_names", cwd="/r", patterns=["refs/heads"], io={"git_out": "main\nfeat\n"})
add("_ref_names", cwd="/r", patterns=["refs/heads"], io={"git_out": ""})
add("_ref_names", cwd="/r", patterns=["refs/heads"], io={"git_out": "  main  \n\n  feat \n"})
add("_ref_names", cwd="/r", patterns=["refs/heads", "refs/remotes"],
    fmt="%(refname:short)\t%(committerdate:iso)", io={"git_out": "main\td1\norigin/main\td2\n"})
add("_ref_names", cwd="/r", patterns=["refs/heads"], io={"git_out": raise_("RuntimeError", "rn")})
add("_ref_names", cwd="/r", patterns=["refs/heads"], io={})

# -- branch_list -----------------------------------------------------------
add("branch_list", cwd="/r", io={"git_out": ""})
add("branch_list", cwd="/r",
    io={"git_out": ["main\nfeat", "worktree /r\nbranch refs/heads/main\n", "main",
                    "origin/main\norigin/feat\norigin/HEAD"]})
add("branch_list", cwd="/r",
    io={"git_out": ["main", "worktree /r\nbranch refs/heads/main\n", "main", "origin/other"]})
add("branch_list", cwd="/r", io={"git_out": ["main", raise_("RuntimeError", "wt")]})
add("branch_list", cwd="/r", io={})
add("branch_list", cwd="/r",
    io={"git_out": ["main\nfeat", "worktree /r\n", "main", "origin/feat2"]})

# -- branch_switch ---------------------------------------------------------
add("branch_switch", cwd="/r", branch="feat", io={"sanitize_branch": "feat", "git_ok": None})
add("branch_switch", cwd="/r", branch="bad", io={"sanitize_branch": ""})
add("branch_switch", cwd="/r", branch="x", io={"sanitize_branch": raise_("RuntimeError", "sb")})
add("branch_switch", cwd="/r", branch="feat",
    io={"sanitize_branch": "feat", "git_ok": raise_("RuntimeError", "switch fail")})
add("branch_switch", cwd="/r", branch="feat", io={})
add("branch_switch", cwd="/r", branch="feat",
    io={"sanitize_branch": "feat", "git_ok": raise_("SystemExit", "s")})

# -- base_branch_list ------------------------------------------------------
add("base_branch_list", cwd="/r", io={"git_out": ""})
add("base_branch_list", cwd="/r", io={"git_out": ["main\td1\norigin/main\td2", "main"]})
add("base_branch_list", cwd="/r", io={"git_out": ["main\td1", "", "trunk"]})
add("base_branch_list", cwd="/r", io={"git_out": ["origin/main\td1", "origin/main"]})
add("base_branch_list", cwd="/r", io={"git_out": raise_("RuntimeError", "bb")})
add("base_branch_list", cwd="/r", io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    # per-function coverage check (>=5 each)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    if short:
        raise SystemExit(f"FATAL: functions with <5 cases: {short}")
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases across {len(counts)} functions to {OUT}")


if __name__ == "__main__":
    main()