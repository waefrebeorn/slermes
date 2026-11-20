#!/usr/bin/env python3
"""Generate tests/fixtures/plugins_install_flow2.jsonl.

Each line is {"func": <name>, "in": {...with io seam queues...}}. The oracle
(play mode) and the C port consume each named io queue from the front, so both
sides see byte-identical seam data; the port under test is the surrounding flow.
>= 5 cases per function, 21 functions.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve()
OUT = HERE.parent / "fixtures" / "plugins_install_flow2.jsonl"


def R(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


CASES = []


def add(func, inp=None, io=None):
    d = {"func": func, "in": dict(inp or {})}
    if io:
        d["in"]["io"] = dict(io)
    else:
        d["in"].setdefault("io", {})
    CASES.append(d)


def F(pid="hardcoded_secret", sev="critical", cat="secret", file="a.py",
      line=3, match="TOKEN=abc", desc="d"):
    return {"pattern_id": pid, "severity": sev, "category": cat, "file": file,
            "line": line, "match": match, "description": desc}


# 1. _walk -------------------------------------------------------------------
for parts, name in [
    (["a", "b.py"], "b.py"),
    ([".git", "config"], "config"),
    (["node_modules", "x.js"], "x.js"),
    (["src", "mod.py"], "mod.py"),
    (["tests", "test_x.py"], "test_x.py"),
    (["__pycache__", "x.pyc"], "x.pyc"),
]:
    add("_walk", {"entries": [{"parts": parts, "name": name}]})

# 2. _finding ----------------------------------------------------------------
for pid, sev, cat in [
    ("hardcoded_secret", "critical", "secret"),
    ("base64_decode_pipe", "medium", "encoding"),
    ("sudo_usage", "high", "command"),
    ("binary_file", "high", "structural"),
    ("context_exfil", "critical", "prompt_injection"),
    ("curl_pipe_shell", "high", "command"),
]:
    add("_finding", {"pattern_id": pid, "severity": sev, "category": cat,
                     "file": "f.py", "match": "m", "description": "desc"})

# 3. _is_main_guard -----------------------------------------------------------
def guard_node(left, right, op="Eq", nops=1):
    return {"test": {"left": left, "ops": [op] * nops, "comparators": [right]}}

NM = {"type": "Name", "id": "__name__"}
MC = {"type": "Constant", "value": "__main__"}
MCX = {"type": "Constant", "value": "other"}
NMX = {"type": "Name", "id": "x"}
for node in [
    guard_node(NM, MC),
    guard_node(MC, NM),
    guard_node(NM, MC, op="NotEq"),
    guard_node(NM, MC, nops=2),
    guard_node(NMX, MC),
    guard_node(NM, MCX),
]:
    add("_is_main_guard", {"node": node})

# 4. _parse_python_source (full builtin exception hierarchy) -----------------
for rt, ap in [
    ("import os\n", {"__module__": True}),
    (R("OSError"), None),
    (R("FileNotFoundError"), None),
    (R("SyntaxError"), None),
    (R("UnicodeDecodeError"), None),
    ("x=1\n", R("TypeError")),
    ("x=1\n", R("MemoryError")),
]:
    io = {"read_text": [rt]}
    if ap is not None or isinstance(rt, str):
        io["ast_parse"] = [ap if ap is not None else {"__module__": True}]
    add("_parse_python_source", {}, io)

# 5. _main_guard_body_lines ---------------------------------------------------
for tree, nodes in [
    ({"__module__": True}, [{"is_guard": True, "body": [{"lineno": 5, "end_lineno": 5}]}]),
    ({"__module__": True}, [{"is_guard": False, "body": [{"lineno": 9, "end_lineno": 9}]}]),
    (None, [{"is_guard": True, "body": [{"lineno": 5, "end_lineno": 5}]}]),
    ({"__module__": True}, [{"is_guard": True, "body": [{"lineno": 2, "end_lineno": 2}, {"lineno": 7, "end_lineno": 7}]}]),
    ({"__module__": True}, []),
    ({"__module__": True}, [{"is_guard": True, "body": []}]),
]:
    add("_main_guard_body_lines", {}, {"parse_source": [tree], "walk": nodes})

# 6. _declares_non_utf8_source ------------------------------------------------
for rb, enc, lk in [
    (list(b"x=1"), "utf-8", {"name": "utf-8"}),
    (list(b"x=1"), "utf-8-sig", {"name": "utf-8-sig"}),
    (list(b"# coding: latin-1"), "iso-8859-1", {"name": "iso8859-1"}),
    (R("OSError"), None, None),
    (list(b"x"), R("SyntaxError"), None),
    (list(b"x"), "utf-8", R("LookupError")),
    (list(b"x"), "utf-8", {"name": "utf-16"}),
]:
    io = {"read_bytes": [rb]}
    if enc is not None or isinstance(rb, list):
        io["detect_encoding"] = [enc if enc is not None else "utf-8"]
    if lk is not None or isinstance(rb, list):
        io["lookup"] = [lk if lk is not None else {"name": "utf-8"}]
    add("_declares_non_utf8_source", {}, io)

# 7. _docstring_lines ---------------------------------------------------------
for tree, decl, nodes in [
    ({"__module__": True}, False, [{"owner": True, "lineno": 2, "end_lineno": 4, "first": {"value": "doc", "lineno": 2, "end_lineno": 4}}]),
    ({"__module__": True}, False, [{"owner": True, "lineno": 2, "end_lineno": 2, "first": None}]),
    (None, False, [{"owner": True, "lineno": 2, "end_lineno": 4, "first": {"value": "doc"}}]),
    ({"__module__": True}, True, [{"owner": True, "lineno": 2, "end_lineno": 4, "first": {"value": "doc"}}]),
    ({"__module__": True}, False, [
        {"owner": True, "lineno": 2, "end_lineno": 5, "first": {"value": "doc", "lineno": 2, "end_lineno": 5}},
        {"owner": False, "lineno": 3, "end_lineno": 3, "has_body": False, "first_line": 3}]),
    ({"__module__": True}, False, []),
]:
    add("_docstring_lines", {}, {"parse_source": [tree], "declares": [decl], "walk": nodes})

# 8. _docstring_cap -----------------------------------------------------------
for sev, agent in [
    ("critical", True), ("high", True), ("high", False),
    ("medium", False), ("low", True), ("critical", False),
]:
    add("_docstring_cap", {"finding": F(sev=sev)}, {"is_agent_facing": [agent]})

# 9. _context_severity -------------------------------------------------------
INERT_NAMES = ["is_base64_media", "is_loopback_only", "is_loopback_continuation",
    "is_hex_in_char_class", "is_pip_install_in_prose_literal", "is_own_proc_read",
    "is_base64_archive_roundtrip", "is_coin_name_only", "is_trusted_installer_pipe",
    "is_sudo_inert", "is_env_dump_in_safety_clause", "is_repo_root_traversal",
    "is_benign_lazy_os_import", "is_disclosed_config_note", "is_authorized_keys_prose",
    "is_public_client_key"]

def context_case(sev, doc, loc, cap, selfun, jsonpv, prose, test, inert_line,
                 regex_tok, inert_over=None, cat="secret", is_code=True, pid="hardcoded_secret"):
    io = {}
    if doc:
        io["catalog_cap" if loc else "prose_cap"] = [cap]
        io["is_self_uninstall_doc"] = [selfun]
    else:
        io["is_json_prose_value"] = [jsonpv]
        if jsonpv:
            io["prose_cap"] = [prose]
    io["is_test_tree"] = [test]
    if test and cat != "credential_exposure":
        io["is_inert_fixture_line"] = [inert_line]
    if is_code:
        io["is_regex_alternation_token"] = [regex_tok]
    for i, nm in enumerate(INERT_NAMES):
        io[nm] = [bool((inert_over or {}).get(nm, False))]
    add("_context_severity",
        {"finding": F(sev=sev, cat=cat, pid=pid), "rel_path": "a.py", "line": 3,
         "joined": "x", "doc_prose": doc, "is_code": is_code, "locale_catalog": loc}, io)

context_case("critical", False, False, None, False, False, None, False, False, False)
context_case("critical", True, False, "high", False, False, None, False, False, False)
context_case("high", True, True, "medium", True, False, None, False, False, False)
context_case("medium", False, False, None, False, True, "high", False, False, False)
context_case("critical", False, False, None, False, False, None, True, True, False)
context_case("high", False, False, None, False, False, None, False, False, True)
context_case("critical", False, False, None, False, False, None, False, False, False,
             inert_over={"is_loopback_only": True})
context_case("high", False, False, None, False, False, None, False, False, False,
             inert_over={"is_public_client_key": True})

# 10. _inert_context_cap -----------------------------------------------------
for i in range(6):
    over = {}
    if i == 1: over["is_loopback_only"] = True
    if i == 2: over["is_public_client_key"] = True
    if i == 3: over["is_base64_archive_roundtrip"] = True
    if i == 4: over["is_own_proc_read"] = True
    io = {nm: [bool(over.get(nm, False))] for nm in INERT_NAMES}
    pid = "encoded_exfil" if i == 5 else "hardcoded_secret"
    io["is_base64_media"] = [False]
    add("_inert_context_cap",
        {"finding": F(sev="critical", pid=pid), "rel_path": "a.py", "line": 3,
         "joined": "x", "is_code": True, "doc_prose": False, "sev": "critical"}, io)

# 11. _filter_findings -------------------------------------------------------
FILTER_SEAMS = ["main_guard_lines", "is_ci", "is_doc_prose", "docstring_lines",
    "locale_catalog", "file_lines", "logical_line", "context_severity",
    "is_defensive_documentation", "docstring_cap", "comment_severity",
    "is_quoted_injection_phrase"]

def filter_case(rel, fsuf, finding, io_over):
    io = {nm: [] for nm in FILTER_SEAMS}
    for k, v in io_over.items():
        io[k] = v
    add("_filter_findings",
        {"rel_path": rel, "file_suffix": fsuf, "findings": [finding]}, io)

filter_case("a.py", ".py", F(pid="hardcoded_secret", sev="critical"),
    {"main_guard_lines": [[]], "is_ci": [False], "is_doc_prose": [False],
     "docstring_lines": [[]], "locale_catalog": [False],
     "file_lines": [["line0", "TOKEN=abc", "line2"]], "logical_line": ["TOKEN=abc"],
     "context_severity": ["high"], "is_defensive_documentation": [False]})
filter_case("a.py", ".py", F(pid="context_exfil", sev="critical"),
    {"main_guard_lines": [[]], "is_ci": [False], "is_doc_prose": [False],
     "docstring_lines": [[3]], "locale_catalog": [False],
     "file_lines": [["line0", "TOKEN=abc"]], "logical_line": ["TOKEN=abc"],
     "context_severity": ["critical"], "is_defensive_documentation": [True],
     "docstring_cap": ["high"], "is_quoted_injection_phrase": [True]})
filter_case("README.md", ".md", F(pid="agent_config_mod", sev="critical"),
    {"is_ci": [False], "is_doc_prose": [True], "locale_catalog": [False],
     "file_lines": [["x", "y", "z"]], "logical_line": ["z"],
     "context_severity": ["high"], "is_defensive_documentation": [False]})
filter_case("src/a.js", ".js", F(pid="dns_exfil", sev="critical"),
    {"is_ci": [False], "is_doc_prose": [False], "locale_catalog": [False],
     "file_lines": [["x", "y", "z"]], "logical_line": ["z"],
     "context_severity": ["high"], "is_defensive_documentation": [False]})
filter_case("a.py", ".py", F(pid="python_os_environ", sev="critical"),
    {"main_guard_lines": [[]], "is_ci": [False], "is_doc_prose": [False],
     "docstring_lines": [[]], "locale_catalog": [False],
     "file_lines": [["TOKEN=abc"]], "logical_line": ["TOKEN=abc"]})
filter_case("docs/notes.rst", ".rst", F(pid="hardcoded_secret", sev="critical", line=1),
    {"is_ci": [True], "is_doc_prose": [False], "locale_catalog": [False],
     "file_lines": [["TOKEN=abc"]], "logical_line": ["TOKEN=abc"],
     "context_severity": ["medium"], "is_defensive_documentation": [False]})

# 12. _at_most ---------------------------------------------------------------
for sev, cap in [("critical", "medium"), ("high", "high"), ("low", "medium"),
                 ("medium", "low"), ("critical", "critical"), ("high", "low")]:
    add("_at_most", {"severity": sev, "cap": cap})

# 13. _comment_severity ------------------------------------------------------
for sev, agent in [("critical", True), ("high", True), ("high", False),
                   ("medium", False), ("low", True), ("medium", True)]:
    add("_comment_severity", {"finding": F(sev=sev)}, {"is_agent_facing": [agent]})

# 14. _file_lines ------------------------------------------------------------
for v in [["a", "b", "c"], ["x", "y"], R("OSError"), R("UnicodeDecodeError"),
          R("FileNotFoundError"), ["only"]]:
    add("_file_lines", {}, {"read_text": [v]})

# 14b. _is_defensive_documentation -------------------------------------------
for rel, match, dl, line in [
    ("CHANGELOG.md", "", None, 3),
    ("a.py", "# comment", None, 3),
    ("a.py", "#!shebang", None, 3),
    ("a.js", "// note", None, 3),
    ("a.py", "code = 1", None, 3),
    ("a.txt", "# text", None, 3),
    ("docs/a.py", "#: note", [3], 3),
]:
    inp = {"rel_path": rel, "finding": F(match=match, line=line)}
    if dl is not None:
        inp["docstring_lines"] = dl
    add("_is_defensive_documentation", inp)

# 15. _dangerous_findings_summary --------------------------------------------
for finds in [
    [],
    [F(sev="critical", pid="a")],
    [F(sev="critical", pid="b"), F(sev="critical", pid="a")],
    [F(sev="high", pid="a"), F(sev="low", pid="c")],
    [F(sev="critical", pid="z"), F(sev="medium", pid="x"), F(sev="critical", pid="z")],
    [F(sev="critical", pid="crypto_mining"), F(sev="critical", pid="hardcoded_secret")],
]:
    add("_dangerous_findings_summary", {"findings": finds})

# 16. _tracked_bytecode ------------------------------------------------------
for out in [["a.pyc", "b.pyo"], [], R("OSError"), R("subprocess.SubprocessError"),
            R("subprocess.CalledProcessError"), ["x.pyc", ""]]:
    add("_tracked_bytecode", {}, {"subprocess_run": [({"out": out} if isinstance(out, list) else out)]})

# 17. _bytecode_findings -----------------------------------------------------
for walk, tracked in [
    ([{"rel": "a.pyc", "is_file": True}], []),
    ([{"rel": "b.pyo", "is_file": False}], []),
    ([], ["c.pyc"]),
    ([{"rel": "a.pyc", "is_file": True}, {"rel": "b.pyc", "is_file": True}], []),
    ([{"rel": "d.txt", "is_file": True}], []),
    ([], []),
]:
    add("_bytecode_findings", {}, {"walk": [walk], "tracked": [tracked]})

# 18. _check_plugin_structure ------------------------------------------------
for bfinds, entries in [
    ([], [{"rel": "a.py", "is_file": True, "size": 100, "suffix": ".py"}]),
    ([], [{"rel": "b.exe", "is_file": True, "size": 100, "suffix": ".exe"}]),
    ([], [{"rel": "big.py", "is_file": True, "size": 2 * 1024 * 1024, "suffix": ".py"}]),
    ([], [{"rel": "link", "is_symlink": True, "resolve_raise": True}]),
    ([], [{"rel": "esc", "is_symlink": True, "in_root": False, "resolved": "/etc"}]),
    ([F(pid="bytecode_file")], []),
    ([], [{"rel": "x", "is_file": False}]),
]:
    add("_check_plugin_structure", {}, {"bytecode_findings": [bfinds], "walk": [entries]})

# 19. scan_plugin ------------------------------------------------------------
for name, is_dir, structure, walk, verdict, finds in [
    ("plug", True, [], [], "safe", []),
    ("plug", True, [F(sev="critical")], [], "dangerous", [F(sev="critical")]),
    ("plug", True, [], [{"rel": "a.py", "is_file": True, "is_symlink": False}], "caution", [F(sev="high")]),
    ("plug", False, [], [], "safe", []),
    ("plug", True, [], [{"rel": "b.py", "is_file": False}], "safe", []),
    ("plug", True, [F(sev="medium", cat="encoding")], [], "safe", []),
]:
    io = {"scanned_at": ["2026-01-01T00:00:00+00:00"], "is_dir": [is_dir],
          "structure": [structure], "walk": [walk], "verdict": [verdict]}
    if is_dir:
        io["scan_file"] = [[] for _ in walk]
        io["filter_findings"] = [finds for _ in walk]
    add("scan_plugin", {"dir_name": name, "source": "git"}, io)

# 20. should_allow_plugin_install --------------------------------------------
for verdict, findings, force in [
    ("safe", [], False),
    ("caution", [F(), F()], False),
    ("caution", [F()], True),
    ("dangerous", [F(sev="critical", pid="a")], False),
    ("dangerous", [F(sev="critical", pid="b"), F(sev="critical", pid="a")], True),
    ("dangerous", [], False),
]:
    add("should_allow_plugin_install",
        {"result": {"verdict": verdict, "findings": findings}, "force": force})


# sanity: >=5 cases per function
from collections import Counter
counts = Counter(c["func"] for c in CASES)
TARGETS = ["_walk", "_finding", "_is_main_guard", "_parse_python_source",
    "_main_guard_body_lines", "_declares_non_utf8_source", "_docstring_lines",
    "_docstring_cap", "_filter_findings", "_at_most", "_comment_severity",
    "_file_lines", "_context_severity", "_inert_context_cap",
    "_is_defensive_documentation", "_dangerous_findings_summary",
    "_tracked_bytecode", "_bytecode_findings", "_check_plugin_structure",
    "scan_plugin", "should_allow_plugin_install"]
assert set(TARGETS) == set(counts), (set(TARGETS) - set(counts), set(counts) - set(TARGETS))
for f in TARGETS:
    assert counts[f] >= 5, (f, counts[f])

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as fh:
    for c in CASES:
        fh.write(json.dumps(c, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT}")
print("per-func:", dict(sorted(counts.items())))
