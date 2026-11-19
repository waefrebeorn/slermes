#!/usr/bin/env python3
"""Generate tests/fixtures/lsp_tools_flow.jsonl — scripted-seam FLOW cases for
the lsp_tools_flow port (the LSP tools surface: tools/file_operations_lint.py
LSP tier + agent/lsp/deferred.py queue + agent/lsp/sync_diagnostics.py leaves).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides.

Domains (documented in the port header):
- ASCII strings, POSIX paths. self/env/agent/client are objects-or-null.
- svc (get_service) is an object whose keys are method names, or null/raise.
- future is {} (double) or null; future_result scripts are COMPLETE CheckResult
  records {superseded, diagnostics, carry, baseline_text} (or null/raise).
- notes/carry are arrays of {"text": str, "baseline_text": str|null}; null notes
  with a truthy owner raise TypeError. token is an integer id (or null).
- message/category in from_tsserver: str|number|bool; line/offset: number|bool
  or absent. Numeric error-message receivers are INTEGERS (int vs float names).
Run: python tests/gen_lsp_tools_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "lsp_tools_flow.jsonl")
CASES = []


def add(func, inp, seams=None):
    inp = dict(inp)
    inp["seams"] = seams if seams is not None else {}
    CASES.append({"func": func, "in": inp})


def rs(cls, msg="", **extra):
    d = {"__raise__": cls, "msg": msg}
    d.update(extra)
    return d


def note(text, baseline=None):
    return {"text": text, "baseline_text": baseline}


def st(queued=None, pending=None, next_token=0):
    return {"queued": queued or {}, "pending": pending or {}, "next_token": next_token}


def q(owner, path, text, baseline=None):
    return {owner: {path: note(text, baseline)}}


SELF = {"env": {"cwd": "/w"}, "cwd": "/w"}
D = {"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 1}},
     "message": "m", "severity": 1, "source": "typescript"}

# ── lsp_service ──
add("lsp_service", {"self": SELF}, {"lsp_local_only": False})
add("lsp_service", {"self": SELF}, {"lsp_local_only": True, "get_service": None})
add("lsp_service", {"self": SELF}, {"lsp_local_only": True, "get_service": {"enabled_for": None}})
add("lsp_service", {"self": SELF}, {"lsp_local_only": True, "get_service": rs("RuntimeError", "x")})
add("lsp_service", {"self": SELF}, {"lsp_local_only": True, "get_service": rs("KeyboardInterrupt", "")})
add("lsp_service", {"self": SELF}, {"lsp_local_only": True, "get_service": rs("SystemExit", "1")})
add("lsp_service", {"self": SELF}, {"lsp_local_only": rs("RuntimeError", "x")})
add("lsp_service", {"self": SELF}, {"lsp_local_only": None})
add("lsp_service", {"self": SELF}, {"lsp_local_only": True, "get_service": rs("ValueError", "x")})
add("lsp_service", {"self": SELF}, {})  # fail-closed

# ── lsp_call ──
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": False},
    {"lsp_local_only": True, "get_service": None})
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": False},
    {"lsp_local_only": True, "get_service": {"enabled_for": None}, "svc_call": True})
add("lsp_call", {"self": SELF, "method": "handles_extension", "path": "/f.ts", "default": "D"},
    {"lsp_local_only": True, "get_service": {"enabled_for": None}})
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": False},
    {"lsp_local_only": True, "get_service": {"enabled_for": None}, "svc_call": rs("RuntimeError", "x")})
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": False},
    {"lsp_local_only": True, "get_service": {"enabled_for": None}, "svc_call": rs("KeyboardInterrupt", "")})
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": False},
    {"lsp_local_only": True, "get_service": {"enabled_for": None}, "svc_call": rs("SystemExit", "1")})
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": False},
    {"lsp_local_only": False})
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": None},
    {"lsp_local_only": True, "get_service": None})
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": False},
    {"lsp_local_only": True, "get_service": {"enabled_for": None}, "svc_call": {"a": 1}})
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": False},
    {"lsp_local_only": True})  # fail-closed get_service
add("lsp_call", {"self": SELF, "method": "", "path": "/f.ts", "default": "D"},
    {"lsp_local_only": True, "get_service": {}})
add("lsp_call", {"self": SELF, "method": "enabled_for", "path": "/f.ts", "default": "D"},
    {"lsp_local_only": True, "get_service": "x"})

# ── local_workspace_untrusted ──
LWU = {"lsp_local_only": True, "load_config_readonly": {"lsp": {"trusted_workspaces": ["a"]}},
       "parse_trusted_workspaces": ["a"], "operator_workspace_roots": [], "is_trusted_workspace": True}
add("local_workspace_untrusted", {"self": SELF}, {"lsp_local_only": False})
add("local_workspace_untrusted", {"self": SELF}, dict(LWU))
add("local_workspace_untrusted", {"self": SELF}, dict(LWU, is_trusted_workspace=False))
add("local_workspace_untrusted", {"self": SELF}, {"lsp_local_only": True, "load_config_readonly": None})
add("local_workspace_untrusted", {"self": SELF}, {"lsp_local_only": True, "load_config_readonly": "x"})
add("local_workspace_untrusted", {"self": SELF}, {"lsp_local_only": True, "load_config_readonly": [1]})
add("local_workspace_untrusted", {"self": SELF}, {"lsp_local_only": True, "load_config_readonly": 5})
add("local_workspace_untrusted", {"self": SELF}, dict(LWU, load_config_readonly={"lsp": None}))
add("local_workspace_untrusted", {"self": SELF}, dict(LWU, load_config_readonly={}))
add("local_workspace_untrusted", {"self": {"cwd": "/w"}},
    {"lsp_local_only": True, "load_config_readonly": {}, "parse_trusted_workspaces": []})
add("local_workspace_untrusted", {"self": {"env": {"cwd": None}}},
    {"lsp_local_only": True, "load_config_readonly": {}, "parse_trusted_workspaces": []})
add("local_workspace_untrusted", {"self": {"env": {"cwd": "/e"}}}, dict(LWU))
add("local_workspace_untrusted", {"self": {"env": {"cwd": ""}, "cwd": "/w"}}, dict(LWU))
add("local_workspace_untrusted", {"self": SELF}, dict(LWU, is_trusted_workspace=rs("KeyboardInterrupt", "")))
add("local_workspace_untrusted", {"self": SELF}, dict(LWU, parse_trusted_workspaces=rs("RuntimeError", "x")))
add("local_workspace_untrusted", {"self": SELF}, dict(LWU, load_config_readonly=rs("SystemExit", "1")))
add("local_workspace_untrusted", {"self": SELF}, dict(LWU, is_trusted_workspace=None))
add("local_workspace_untrusted", {"self": SELF}, dict(LWU, is_trusted_workspace=""))
add("local_workspace_untrusted", {"self": SELF}, dict(LWU, operator_workspace_roots=rs("ValueError", "x")))
add("local_workspace_untrusted", {"self": SELF}, {"lsp_local_only": True})  # fail-closed

# ── run_managed_node_linter ──
RMN = {"with_hermes_node_path": [{"PATH": "/node"}, {"PATH": "/node2"}],
       "shutil_which": "/node/node", "hermes_subprocess_env": {"PATH": "/usr"},
       "windows_hide_flags": 0, "subprocess_run": {"stdout": "ok", "returncode": 0}}
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    {"with_hermes_node_path": {"PATH": "/n"}, "shutil_which": None})
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"}, dict(RMN))
add("run_managed_node_linter", {"self": SELF, "ext": ".ts", "path": "/f.ts"},
    dict(RMN, subprocess_run={"stdout": "err", "returncode": 1}))
add("run_managed_node_linter", {"self": SELF, "ext": ".go", "path": "/f.go"}, {})
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    dict(RMN, subprocess_run=rs("TimeoutExpired", "", cmd=["node"], timeout=30)))
add("run_managed_node_linter", {"self": SELF, "ext": ".ts", "path": "/f.ts"},
    dict(RMN, subprocess_run=rs("TimeoutExpired", "", cmd=["npx"], timeout=30)))
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    dict(RMN, subprocess_run=rs("OSError", "x")))
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    dict(RMN, subprocess_run=rs("KeyboardInterrupt", "")))
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    {"with_hermes_node_path": None})
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    {"with_hermes_node_path": {}})
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    {"with_hermes_node_path": rs("RuntimeError", "x")})
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    dict(RMN, shutil_which=rs("SystemExit", "1")))
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js", "is_windows": True},
    dict(RMN, msys_to_windows_path=["C:/f.js", "C:/w"]))
add("run_managed_node_linter", {"self": {"env": {"cwd": None}, "cwd": None}, "ext": ".js", "path": "/f.js", "is_windows": True},
    dict(RMN, msys_to_windows_path=["C:/f.js"]))
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    dict(RMN, subprocess_run={"stdout": None, "returncode": 0}))
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    dict(RMN, subprocess_run={"stdout": "x"}))
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    dict(RMN, subprocess_run=None))
add("run_managed_node_linter", {"self": {"cwd": "/w"}, "ext": ".js", "path": "/f.js"}, dict(RMN))
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"}, {})  # fail-closed
add("run_managed_node_linter", {"self": SELF, "ext": "", "path": "/f"}, {})
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    dict(RMN, hermes_subprocess_env=rs("ValueError", "x")))
add("run_managed_node_linter", {"self": SELF, "ext": ".js", "path": "/f.js"},
    dict(RMN, windows_hide_flags=rs("RuntimeError", "x")))

# ── format_lsp_block ──
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [], "header": "H:"}, {})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": None, "header": "H:"}, {})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": "block", "truncate": "T"})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": ""})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": None})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": rs("RuntimeError", "x"), "logger_debug": None})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": rs("KeyboardInterrupt", "")})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": "b", "truncate": rs("ValueError", "x"), "logger_debug": None})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": "b", "truncate": rs("SystemExit", "1")})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": 5, "logger_debug": None})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": rs("RuntimeError", "x"), "logger_debug": rs("RuntimeError", "log")})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": "b", "truncate": None})
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"}, {})  # fail-closed
add("format_lsp_block", {"path": "/f.ts", "diagnostics": [D], "header": "H:"},
    {"report_for_file": "b", "truncate": rs("KeyboardInterrupt", "")})

# ── post_deferred ──
CR = {"superseded": False, "diagnostics": [D], "carry": [], "baseline_text": "b"}
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "token": 1, "state": st()},
    {"future_cancelled": True, "settled_notify_all": None})
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "token": 1, "state": st()},
    {"future_cancelled": False, "future_result": CR, "report_for_file": "blk", "truncate": "T", "settled_notify_all": None})
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "token": 1, "state": st()},
    {"future_cancelled": False, "future_result": dict(CR, superseded=True), "settled_notify_all": None})
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "token": 1, "state": st()},
    {"future_cancelled": False, "future_result": dict(CR, diagnostics=None, carry=[note("old", "ob")]), "settled_notify_all": None})
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": None, "carry": [note("c", "cb")], "token": 1, "state": st()},
    {"logger_debug": None, "settled_notify_all": None})
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "token": 1, "state": st()},
    {"future_cancelled": False, "future_result": rs("RuntimeError", "x"), "logger_debug": None, "settled_notify_all": None})
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "token": 1, "state": st()},
    {"future_cancelled": False, "future_result": rs("KeyboardInterrupt", ""), "settled_notify_all": None})
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "token": 1, "state": st()},
    {"future_cancelled": True, "settled_notify_all": rs("RuntimeError", "x")})
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "token": 1, "state": st()},
    {"future_cancelled": False, "future_result": rs("KeyboardInterrupt", ""), "settled_notify_all": rs("RuntimeError", "reset")})
add("post_deferred", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "token": 1, "state": st()},
    {"logger_debug": None, "settled_notify_all": None})  # fail-closed future_cancelled

# ── queue_late_verdict ──
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": True})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": CR, "report_for_file": "b", "truncate": "T"})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": dict(CR, superseded=True)})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": dict(CR, diagnostics=None, carry=[note("old", "ob")])})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": None, "carry": [note("c", "cb")], "state": st()},
    {"logger_debug": None})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": rs("ValueError", "x"), "logger_debug": None})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": rs("KeyboardInterrupt", "")})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": {"diagnostics": [D], "carry": [], "baseline_text": "b"}})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": dict(CR, diagnostics=None, carry=None)})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": CR, "report_for_file": rs("KeyboardInterrupt", "")})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": "x"})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()}, {})  # fail-closed
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": CR, "report_for_file": "b", "truncate": ""})
add("queue_late_verdict", {"owner": "o", "abs_path": "/f.ts", "future": {}, "carry": [], "state": st()},
    {"future_cancelled": False, "future_result": None})

# ── bind_owner ──
add("bind_owner", {"owner": "s"}, {"owner_set": "tok", "owner_reset": None})
add("bind_owner", {"owner": None}, {"owner_set": "tok", "owner_reset": None})
add("bind_owner", {"owner": ""}, {"owner_set": "tok", "owner_reset": None})
add("bind_owner", {"owner": "s", "body": {"read": True}}, {"owner_set": "tok", "owner_get": "s", "owner_reset": None})
add("bind_owner", {"owner": "s", "body": {"raise": rs("KeyboardInterrupt", "")}}, {"owner_set": "tok", "owner_reset": None})
add("bind_owner", {"owner": "s", "body": {"raise": rs("RuntimeError", "x")}}, {"owner_set": "tok", "owner_reset": None})
add("bind_owner", {"owner": "s"}, {"owner_set": rs("RuntimeError", "x")})
add("bind_owner", {"owner": "s"}, {"owner_set": "tok", "owner_reset": rs("RuntimeError", "x")})
add("bind_owner", {"owner": "s", "body": {"raise": rs("ValueError", "body")}}, {"owner_set": "tok", "owner_reset": rs("TypeError", "reset")})
add("bind_owner", {"owner": "s"}, {"owner_set": rs("SystemExit", "1")})
add("bind_owner", {"owner": "s", "body": {"read": True}}, {"owner_set": "tok", "owner_get": rs("RuntimeError", "x"), "owner_reset": None})
add("bind_owner", {"owner": "s"}, {})  # fail-closed

# ── current_owner ──
add("current_owner", {}, {"owner_get": "s"})
add("current_owner", {}, {"owner_get": None})
add("current_owner", {}, {"owner_get": rs("RuntimeError", "x")})
add("current_owner", {}, {"owner_get": rs("KeyboardInterrupt", "")})
add("current_owner", {}, {"owner_get": 5})
add("current_owner", {}, {})  # fail-closed

# ── deferred_post ──
add("deferred_post", {"owner": "o", "path": "/p", "text": "t", "baseline_text": "b", "state": st()})
add("deferred_post", {"owner": "o", "path": "/p", "text": "t", "baseline_text": None, "state": st()})
add("deferred_post", {"owner": "o", "path": "/p", "text": "new", "baseline_text": "nb", "state": st(q("o", "/p", "old", "ob"))})
add("deferred_post", {"owner": "o", "path": "/p", "text": "t", "baseline_text": "nb", "state": st(q("o", "/p", "t", "ob"))})
add("deferred_post", {"owner": "o", "path": "/p2", "text": "t2", "baseline_text": None, "state": st(q("o", "/p1", "t1"))})
add("deferred_post", {"owner": "o1", "path": "/p", "text": "t", "baseline_text": None,
                      "state": st({"o1": {"/a": note("a")}, "o2": {"/b": note("b")}})})
# per-owner cap (33 paths -> evict oldest, keep 32)
_big = {"/p%02d" % i: note("t%d" % i) for i in range(33)}
add("deferred_post", {"owner": "o", "path": "/new", "text": "n", "baseline_text": None, "state": st({"o": _big})})
# owner cap (65 owners -> evict oldest, keep 64)
_many = {"o%02d" % i: {"/p": note("t")} for i in range(65)}
add("deferred_post", {"owner": "new", "path": "/p", "text": "t", "baseline_text": None, "state": st(_many)})
add("deferred_post", {"owner": "", "path": "/p", "text": "t", "baseline_text": None, "state": st()})
add("deferred_post", {"owner": "o", "path": "/p", "text": "t", "baseline_text": None, "state": st({"o": {}})})

# ── deferred_take ──
add("deferred_take", {"owner": None, "path": "/p", "state": st(q("o", "/p", "t"))})
add("deferred_take", {"owner": "", "path": "/p", "state": st(q("o", "/p", "t"))})
add("deferred_take", {"owner": "o", "path": "/p", "state": st(q("o", "/p", "t", "b"))})
add("deferred_take", {"owner": "o", "path": "/absent", "state": st(q("o", "/p", "t"))})
add("deferred_take", {"owner": "absent", "path": "/p", "state": st(q("o", "/p", "t"))})
add("deferred_take", {"owner": "o", "path": "/p", "state": st({"o": {}})})
add("deferred_take", {"owner": "o", "path": "/p", "state": st({"o": {"/p": note("t"), "/q": note("u")}})})

# ── deferred_restore ──
add("deferred_restore", {"owner": "o", "path": "/p", "notes": [note("t", "b")], "state": st()})
add("deferred_restore", {"owner": None, "path": "/p", "notes": [note("t")], "state": st()})
add("deferred_restore", {"owner": "", "path": "/p", "notes": [note("t")], "state": st()})
add("deferred_restore", {"owner": "o", "path": "/p", "notes": [], "state": st()})
add("deferred_restore", {"owner": "o", "path": "/p", "notes": None, "state": st()})
add("deferred_restore", {"owner": "o", "path": "/p", "notes": [note("n1", "b1"), note("n2", "b2")], "state": st()})
add("deferred_restore", {"owner": None, "path": "/p", "notes": None, "state": st()})

# ── pop_notes ──
add("pop_notes", {"owners": ["o"], "state": st({"o": {"/p1": note("t1"), "/p2": note("t2")}})})
add("pop_notes", {"owners": ["o", "o"], "state": st(q("o", "/p", "t"))})
add("pop_notes", {"owners": ["o1", "o2"], "state": st({"o1": {"/a": note("a")}, "o2": {"/b": note("b")}})})
add("pop_notes", {"owners": [None, ""], "state": st(q("o", "/p", "t"))})
add("pop_notes", {"owners": ["absent"], "state": st(q("o", "/p", "t"))})
add("pop_notes", {"owners": [], "state": st(q("o", "/p", "t"))})
add("pop_notes", {"owners": ["o", None, "o"], "state": st(q("o", "/p", "t"))})
add("pop_notes", {"owners": ["o"], "state": st({"o": {}})})

# ── track_pending ──
add("track_pending", {"owner": "o", "state": st()})
add("track_pending", {"owner": "o", "state": st(next_token=5)})
add("track_pending", {"owner": "o", "state": st(pending={"o": [1]}, next_token=5)})
add("track_pending", {"owner": "", "state": st()})
add("track_pending", {"owner": "o2", "state": st(pending={"o1": [0]}, next_token=1)})

# ── settled ──
add("settled", {"owner": "o", "token": 1, "state": st(pending={"o": [1, 2]})}, {"settled_notify_all": None})
add("settled", {"owner": "o", "token": 2, "state": st(pending={"o": [1, 2]})}, {"settled_notify_all": None})
add("settled", {"owner": "o", "token": 1, "state": st(pending={"o": [1]})}, {"settled_notify_all": None})
add("settled", {"owner": "o", "token": 9, "state": st(pending={"o": [1]})}, {"settled_notify_all": None})
add("settled", {"owner": "absent", "token": 1, "state": st(pending={"o": [1]})}, {"settled_notify_all": None})
add("settled", {"owner": "o", "token": None, "state": st(pending={"o": [1]})}, {"settled_notify_all": None})
add("settled", {"owner": "o", "token": 1, "state": st(pending={"o": [1]})}, {"settled_notify_all": rs("RuntimeError", "x")})
add("settled", {"owner": "o", "token": 1, "state": st(pending={"o": [1]})}, {"settled_notify_all": rs("KeyboardInterrupt", "")})
add("settled", {"owner": "o", "token": 1, "state": st(pending={"o": [1, 2]})}, {"settled_notify_all": rs("ValueError", "x")})

# ── owners ──
add("owners", {"agent": {"session_id": "s", "_current_task_id": "t"}})
add("owners", {"agent": {"session_id": "s"}})
add("owners", {"agent": {}})
add("owners", {"agent": None})
add("owners", {"agent": {"session_id": 5, "_current_task_id": True}})
add("owners", {"agent": {"session_id": "", "_current_task_id": "t"}})
add("owners", {"agent": {"session_id": None, "_current_task_id": "t"}})

# ── settle_for_agent ──
add("settle_for_agent", {"agent": {}, "state": st()})
add("settle_for_agent", {"agent": {"session_id": "s"}, "state": st(q("s", "/p", "t"), pending={"s": [1]})},
    {"settled_wait_for": True})
add("settle_for_agent", {"agent": {"session_id": "s"}, "state": st(pending={})}, {"settled_wait_for": True})
add("settle_for_agent", {"agent": {"session_id": "s", "_current_task_id": "s"}, "state": st(q("s", "/p", "t"))},
    {"settled_wait_for": True})
add("settle_for_agent", {"agent": {"session_id": "s", "_current_task_id": "t"},
                         "state": st({"s": {"/a": note("a")}, "t": {"/b": note("b")}}, pending={"s": [1]})},
    {"settled_wait_for": False})
add("settle_for_agent", {"agent": {"session_id": "s"}, "state": st()}, {"settled_wait_for": rs("RuntimeError", "x")})
add("settle_for_agent", {"agent": {"session_id": "s"}, "state": st()}, {"settled_wait_for": rs("KeyboardInterrupt", "")})
add("settle_for_agent", {"agent": {"session_id": "s"}, "timeout": 0.5, "state": st()}, {"settled_wait_for": True})
add("settle_for_agent", {"agent": None, "state": st()})
add("settle_for_agent", {"agent": {"session_id": "s"}, "state": st(pending={"s": []})}, {"settled_wait_for": True})

# ── forward_to_agent ──
add("forward_to_agent", {"agent": {"session_id": "s"}, "state": st(q("s", "/p", "t"))},
    {"queue_plugin_notice": None})
add("forward_to_agent", {"agent": {}, "state": st(q("s", "/p", "t"))})
add("forward_to_agent", {"agent": {"session_id": "s"}, "state": st()})
add("forward_to_agent", {"agent": {"session_id": "s"}, "state": st({"s": {"/p1": note("t1"), "/p2": note("t2")}})},
    {"queue_plugin_notice": None})
add("forward_to_agent", {"agent": {"session_id": "s"}, "state": st(q("s", "/p", "t"))},
    {"queue_plugin_notice": rs("RuntimeError", "x")})
add("forward_to_agent", {"agent": {"session_id": "s"}, "state": st(q("s", "/p", "t"))},
    {"queue_plugin_notice": rs("KeyboardInterrupt", "")})
add("forward_to_agent", {"agent": {"session_id": "s", "_current_task_id": "t"},
                         "state": st({"s": {"/a": note("a")}, "t": {"/b": note("b")}})},
    {"queue_plugin_notice": None})
add("forward_to_agent", {"agent": None, "state": st(q("s", "/p", "t"))})

# ── sync_supported ──
add("sync_supported", {"client": {"server_id": "typescript"}})
add("sync_supported", {"client": {"server_id": "typescript", "_hermes_sync_diagnostics_unsupported": True}})
add("sync_supported", {"client": {"server_id": "typescript", "_hermes_sync_diagnostics_unsupported": False}})
add("sync_supported", {"client": {"server_id": "pyright"}})
add("sync_supported", {"client": {"server_id": None}})
add("sync_supported", {"client": {}})
add("sync_supported", {"client": None})
add("sync_supported", {"client": {"server_id": "typescript", "_hermes_sync_diagnostics_unsupported": 1}})
add("sync_supported", {"client": {"server_id": "typescript", "_hermes_sync_diagnostics_unsupported": ""}})
add("sync_supported", {"client": "x"})

# ── from_tsserver ──
add("from_tsserver", {"item": None})
add("from_tsserver", {"item": "x"})
add("from_tsserver", {"item": [1]})
add("from_tsserver", {"item": {}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}}})
add("from_tsserver", {"item": {"startLocation": {"line": 2, "offset": 3}, "endLocation": {"line": 4, "offset": 5},
                               "message": "m", "category": "warning", "source": "ts", "code": 2304}})
for cat, sev in [("error", 1), ("message", 3), ("suggestion", 4), ("unknown", 1)]:
    add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "category": cat}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "message": None}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "message": 0}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "message": 5}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "message": True}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "category": None}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "source": None}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "source": ""}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "source": 5}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "code": None}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "code": 0}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {}, "code": False}})
add("from_tsserver", {"item": {"startLocation": None, "endLocation": {}}})
add("from_tsserver", {"item": {"startLocation": {"line": None}, "endLocation": {}}})
add("from_tsserver", {"item": {"startLocation": {"line": "x"}, "endLocation": {}}})
add("from_tsserver", {"item": {"startLocation": {"line": 2.0}, "endLocation": {}}})
add("from_tsserver", {"item": {"startLocation": {"line": True}, "endLocation": {}}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": {"offset": [1]}}})
add("from_tsserver", {"item": {"startLocation": {}, "endLocation": None}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
