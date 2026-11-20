#!/usr/bin/env python3
"""Generate tests/fixtures/context_references_flow3.jsonl — >=5 cases per ported
function (11 funcs), covering decision branches, error paths, the FULL builtin
exception hierarchy (Exception children fold, BaseException trio + CancelledError
propagate), real composition and fail-closed missing seams.

Script semantics match harness/oracle (FAIL-CLOSED): a MISSING script is a
RuntimeError raise spec; an array is a per-call queue (last repeats); a single
value is a constant; a scripted null is None; {"__raise__":cls,"msg":...} is an
exception (built as a REAL builtin class so `except` binds by hierarchy identity).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "context_references_flow3.jsonl")

CASES = []

# Seams whose LIVE return is a LIST/iterable: the universal seam treats a bare
# array script as a PER-CALL QUEUE, so the desired list must be one queue slot.
LIST_SEAMS = {
    "builtin_matches", "plugin_matches", "parse_refs", "harden_argv",
    "composer_roots", "os_walk", "iter_entries",
}


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def add(func, inp=None, seams=None, **extra):
    d = dict(inp or {})
    d.update(extra)
    seams = dict(seams or {})
    for k in LIST_SEAMS:
        v = seams.get(k)
        if isinstance(v, list):
            seams[k] = [v]  # one queue slot holding the whole list
    if func == "ensure_path_allowed":
        # live: Path(expanduser("~")).resolve(), get_hermes_home().resolve()
        seams.setdefault("resolve_path", [d.get("home", ""), d.get("hermes_home", "")])
    d["seams"] = seams
    CASES.append({"func": func, "in": d})


def M(g0, start, end, simple=None, kind=None, value=None):
    d = {"g0": g0, "start": start, "end": end}
    if simple is not None:
        d["simple"] = simple
    if kind is not None:
        d["kind"] = kind
    if value is not None:
        d["value"] = value
    return d


def ref(raw, kind, target, start=0, end=1, ls=None, le=None):
    return {"raw": raw, "kind": kind, "target": target, "start": start, "end": end,
            "line_start": ls, "line_end": le}


def result(message="", original=None, refs=None, warnings=None, tokens=0,
           expanded=False, blocked=False):
    return {"message": message, "original_message": message if original is None else original,
            "references": refs or [], "warnings": warnings or [],
            "injected_tokens": tokens, "expanded": expanded, "blocked": blocked}


# ── F1 parse_context_references ──
add("parse_context_references", {"message": ""})
add("parse_context_references", {"message": "see @diff now"},
    {"builtin_matches": [M("@diff", 4, 9, simple="diff")]})
add("parse_context_references", {"message": "read @file:x.py:10-20"},
    {"builtin_matches": [M("@file:x.py:10-20", 5, 21, kind="file", value="x.py:10-20")]})
add("parse_context_references", {"message": "see @url:example.com"},
    {"builtin_matches": [M("@url:example.com", 4, 20, kind="url", value="example.com")]})
add("parse_context_references", {"message": "file (see @file:x.py)"},
    {"builtin_matches": [M("@file:x.py)", 9, 19, kind="file", value="x.py)")]})
add("parse_context_references", {"message": "@issue:123",
                                 "providers": {"issue": {"prefix": "issue"}}},
    {"builtin_matches": [],
     "plugin_matches": [M("@issue:123", 0, 10, kind="issue", value="123")]})
add("parse_context_references", {"message": "@issue:123 @issue:123",
                                 "providers": {"issue": {"prefix": "issue"}}},
    {"builtin_matches": [M("@issue:123", 0, 10, kind="issue", value="123")],
     "plugin_matches": [M("@issue:123", 0, 10, kind="issue", value="123"),
                        M("@issue:123", 11, 21, kind="issue", value="123")]})
add("parse_context_references", {"providers": {"issue": {}}})
add("parse_context_references", {"message": "@diff", "plugins": False,
                                 "providers": {"issue": {}}},
    {"builtin_matches": [M("@diff", 0, 5, simple="diff")]})
add("parse_context_references", {"message": "@diff"},
    {"builtin_matches": R("SystemExit", "propagate")})

# ── F2 preprocess (sync wrapper) ──
add("preprocess", {"message": "hi", "cwd": "/w", "context_length": 100},
    {"get_running_loop": None,
     "preprocess_async_result": result("hi", refs=[ref("@diff", "diff", "")])})
add("preprocess", {"message": "hi", "cwd": "/w", "context_length": 10},
    {"get_running_loop": R("RuntimeError", "no running loop"),
     "preprocess_async_result": result("hi", blocked=True)})
add("preprocess", {"message": "hi"},
    {"get_running_loop": R("ValueError", "bad")})
add("preprocess", {"message": "hi"},
    {"get_running_loop": R("SystemExit", "bye")})
add("preprocess", {"message": "hi"},
    {"get_running_loop": None,
     "preprocess_async_result": R("ValueError", "async boom")})
add("preprocess", {"message": "hi"})

# ── F3 preprocess_context_references_async ──
R1 = ref("@diff", "diff", "")
add("preprocess_async", {"message": "hi", "cwd": "/w", "context_length": 100},
    {"parse_refs": []})
add("preprocess_async", {"message": "hi", "cwd": "/w", "context_length": 100},
    {"parse_refs": [R1], "host_refusal": "REFUSED"})
add("preprocess_async", {"message": "see @diff", "cwd": "/w", "context_length": 100},
    {"parse_refs": [R1], "host_refusal": None,
     "resolve_path": "/w", "expand_ref": [[None, "BLOCK"]], "estimate_tokens": [5]})
add("preprocess_async", {"message": "see @diff", "cwd": "/w", "context_length": 100},
    {"parse_refs": [R1], "host_refusal": None,
     "resolve_path": "/w", "expand_ref": [["a warning", None]], "estimate_tokens": []})
add("preprocess_async", {"message": "hi", "cwd": "/w", "context_length": 10},
    {"parse_refs": [R1], "host_refusal": None,
     "resolve_path": "/w", "expand_ref": [[None, "BIG"]], "estimate_tokens": [10]})
add("preprocess_async", {"message": "hi", "cwd": "/w", "context_length": 20},
    {"parse_refs": [R1], "host_refusal": None,
     "resolve_path": "/w", "expand_ref": [[None, "MED"]], "estimate_tokens": [7]})
add("preprocess_async", {"message": "hi", "cwd": "/w", "context_length": 100,
                         "allowed_root": "/allowed"},
    {"parse_refs": [R1], "host_refusal": None,
     "resolve_path": ["/w", "/allowed"], "expand_ref": [[None, "OK"]],
     "estimate_tokens": [1]})
add("preprocess_async", {"message": "hi", "cwd": "/w", "context_length": 100},
    {"parse_refs": [R1], "host_refusal": None, "resolve_path": "/w",
     "expand_ref": R("SystemExit", "propagate")})
add("preprocess_async", {"message": "hi", "cwd": "/w", "context_length": 100})

# ── F4 _expand_reference ──
add("expand_reference", {"ref": ref("@file:x.py", "file", "x.py"), "cwd": "/w"},
    {"expand_path": [None, "FILE BLOCK"]})
add("expand_reference", {"ref": ref("@diff", "diff", ""), "cwd": "/w"},
    {"expand_git": [None, "GIT BLOCK"]})
add("expand_reference", {"ref": ref("@git:15", "git", "15"), "cwd": "/w"},
    {"expand_git": [None, "GIT BLOCK"]})
add("expand_reference", {"ref": ref("@url:x", "url", "x"), "cwd": "/w"},
    {"fetch_url": "hello world", "estimate_tokens": 3})
add("expand_reference", {"ref": ref("@url:x", "url", "x"), "cwd": "/w"},
    {"fetch_url": ""})
add("expand_reference", {"ref": ref("@thing:y", "thing", "y"), "cwd": "/w"})
add("expand_reference", {"ref": ref("@issue:7", "issue", "7"), "cwd": "/w",
                         "providers": {"issue": {}}},
    {"provider_expand": "plugin text", "estimate_tokens": 2})
add("expand_reference", {"ref": ref("@file:x.py", "file", "x.py"), "cwd": "/w"},
    {"expand_path": R("ValueError", "bad path")})
add("expand_reference", {"ref": ref("@file:x.py", "file", "x.py"), "cwd": "/w"},
    {"expand_path": R("SystemExit", "bye")})
add("expand_reference", {"ref": ref("@thing:y", "thing", "y"), "cwd": "/w",
                         "providers": {"thing": {}}},
    {"provider_expand": R("ValueError", "plugin error")})

# ── F5 _expand_git_reference ──
GR = ref("@diff", "diff", "")
add("expand_git_reference", {"ref": GR, "cwd": "/w", "args": ["diff"], "label": "git diff"},
    {"noninteractive_env": None})
add("expand_git_reference", {"ref": GR, "cwd": "/w", "args": ["diff"], "label": "git diff"},
    {"noninteractive_env": {}, "harden_argv": ["diff"],
     "run_quiet": {"returncode": 0, "stdout": "patch\n", "stderr": ""},
     "estimate_tokens": 3})
add("expand_git_reference", {"ref": GR, "cwd": "/w", "args": ["diff"], "label": "git diff"},
    {"noninteractive_env": {}, "harden_argv": ["diff"],
     "run_quiet": {"returncode": 1, "stdout": "", "stderr": "fatal\n"}})
add("expand_git_reference", {"ref": GR, "cwd": "/w", "args": ["diff"], "label": "git diff"},
    {"noninteractive_env": {}, "harden_argv": ["diff"],
     "run_quiet": {"returncode": 1, "stdout": "", "stderr": ""}})
add("expand_git_reference", {"ref": GR, "cwd": "/w", "args": ["diff"], "label": "git diff"},
    {"noninteractive_env": {}, "harden_argv": ["diff"],
     "run_quiet": {"returncode": 0, "stdout": "", "stderr": ""}, "estimate_tokens": 1})
add("expand_git_reference", {"ref": GR, "cwd": "/w", "args": ["diff"], "label": "git diff"},
    {"noninteractive_env": {}, "harden_argv": ["diff"],
     "run_quiet": R("TimeoutExpired", "timed out")})
add("expand_git_reference", {"ref": GR, "cwd": "/w", "args": ["diff"], "label": "git diff"},
    {"noninteractive_env": {}, "harden_argv": ["diff"],
     "run_quiet": R("SystemExit", "bye")})
add("expand_git_reference", {"ref": GR, "cwd": "/w", "args": ["diff"], "label": "git diff"})

# ── F6 _fetch_url_content ──
add("fetch_url_content", {"url": "https://x"}, {"fetch": "  hello  "})
add("fetch_url_content", {"url": "https://x"}, {"fetch": None})
add("fetch_url_content", {"url": "https://x"}, {"fetch": {"__await__": "  async  "}})
add("fetch_url_content", {"url": "https://x"}, {"fetch": 42})
add("fetch_url_content", {"url": "https://x"}, {"fetch": False})
add("fetch_url_content", {"url": "https://x"}, {"fetch": R("ValueError", "bad url")})
add("fetch_url_content", {"url": "https://x"})

# ── F7 _default_url_fetcher ──
add("default_url_fetcher", {"url": "https://x"},
    {"web_extract": '{"results":[{"content":"  hi  "}]}'})
add("default_url_fetcher", {"url": "https://x"},
    {"web_extract": '{"results":[],"error":"refused"}'})
add("default_url_fetcher", {"url": "https://x"},
    {"web_extract": '{"results":[]}'})
add("default_url_fetcher", {"url": "https://x"},
    {"web_extract": '{"results":[{"blocked_by_policy":true,"error":"blocked"}]}'})
add("default_url_fetcher", {"url": "https://x"},
    {"web_extract": '{"results":[{"content":"","raw_content":" raw "}]}'})
add("default_url_fetcher", {"url": "https://x"},
    {"web_extract": '{"results":[{"content":"","error":"ssrf"}]}'})
add("default_url_fetcher", {"url": "https://x"},
    {"web_extract": R("ValueError", "fetch boom")})
add("default_url_fetcher", {"url": "https://x"})

# ── F8 _resolve_path ──
add("resolve_path", {"cwd": "/w", "target": "\\\\?\\C:\\secret"},
    {"is_nt_namespace": True})
add("resolve_path", {"cwd": "/w", "target": "x.py"},
    {"is_nt_namespace": False, "resolve_path": "/w/x.py"})
add("resolve_path", {"cwd": "/w", "target": "x.py", "allowed_root": "/w"},
    {"is_nt_namespace": False, "resolve_path": "/w/x.py", "is_under": [True]})
add("resolve_path", {"cwd": "/w", "target": "x.py", "allowed_root": "/allowed"},
    {"is_nt_namespace": False, "resolve_path": "/w/x.py",
     "is_under": [False, True], "composer_roots": ["/cache"]})
add("resolve_path", {"cwd": "/w", "target": "x.py", "allowed_root": "/allowed"},
    {"is_nt_namespace": False, "resolve_path": "/w/x.py",
     "is_under": [False, False], "composer_roots": [], "agent_staged": True})
add("resolve_path", {"cwd": "/w", "target": "x.py", "allowed_root": "/allowed"},
    {"is_nt_namespace": False, "resolve_path": "/w/x.py",
     "is_under": [False, False], "composer_roots": [], "agent_staged": False})
add("resolve_path", {"cwd": "/w", "target": "x.py"})

# ── F9 _ensure_reference_path_allowed ──
CONSTS = {"sensitive_home_files": [".ssh/id_rsa"],
          "sensitive_hermes_dirs": ["skills/.hub"],
          "credential_dirs": [".ssh"]}
add("ensure_path_allowed", {"path": "/h/x.py", "home": "/h", "hermes_home": "/h/.hermes",
                            "constants": CONSTS},
    {"read_block": None})
add("ensure_path_allowed", {"path": "/h/.ssh/id_rsa", "home": "/h",
                            "hermes_home": "/h/.hermes", "constants": CONSTS},
    {"read_block": None})
add("ensure_path_allowed", {"path": "/h/.ssh/config", "home": "/h",
                            "hermes_home": "/h/.hermes", "constants": CONSTS},
    {"read_block": None})
add("ensure_path_allowed", {"path": "/h/.hermes/.env", "home": "/h",
                            "hermes_home": "/h/.hermes", "constants": CONSTS},
    {"read_block": None})
add("ensure_path_allowed", {"path": "/h/x.py", "home": "/h", "hermes_home": "/h/.hermes",
                            "constants": CONSTS},
    {"read_block": "auth.json blocked"})
add("ensure_path_allowed", {"path": "/h/x.py", "home": "/h", "hermes_home": "/h/.hermes",
                            "constants": CONSTS},
    {"read_block": R("ValueError", "canonical deny")})
add("ensure_path_allowed", {"path": "/h/x.py", "home": "/h", "hermes_home": "/h/.hermes",
                            "constants": CONSTS},
    {"read_block": R("RuntimeError", "lookup failed")})
add("ensure_path_allowed", {"path": "/h/x.py", "home": "/h", "hermes_home": "/h/.hermes",
                            "constants": CONSTS},
    {"read_block": R("SystemExit", "bye")})
add("ensure_path_allowed", {"path": "/h/x.py", "home": "/h", "hermes_home": "/h/.hermes",
                            "constants": CONSTS})

# ── F11 _iter_visible_entries ──
add("iter_visible_entries", {"path": "/w/dir", "cwd": "/w"},
    {"run_rg": {"returncode": 0, "stdout": "dir/a.py\ndir/sub/b.py\n"},
     "exists": [True], "is_dir": [True, False, False, False]})
add("iter_visible_entries", {"path": "/w/dir", "cwd": "/w"},
    {"run_rg": {"returncode": 0, "stdout": ""}})
add("iter_visible_entries", {"path": "/w/dir", "cwd": "/w"},
    {"run_rg": {"returncode": 1, "stdout": "", "stderr": "no rg"},
     "os_walk": [{"root": "/w/dir", "dirs": ["sub", ".hidden", "__pycache__"],
                  "files": ["a.py", ".dot"]}]})
add("iter_visible_entries", {"path": "/w/dir", "cwd": "/w", "limit": 2},
    {"run_rg": {"returncode": 1, "stdout": ""},
     "os_walk": [{"root": "/w/dir", "dirs": ["s1", "s2", "s3"], "files": ["f1", "f2"]}]})
add("iter_visible_entries", {"path": "/w/dir", "cwd": "/w"},
    {"run_rg": R("FileNotFoundError", "no rg")})
add("iter_visible_entries", {"path": "/w/dir", "cwd": "/w"},
    {"run_rg": R("SystemExit", "bye")})
add("iter_visible_entries", {"path": "/w/dir", "cwd": "/w"})

# ── F10 _build_folder_listing ──
add("build_folder_listing", {"path": "/w/dir", "cwd": "/w"},
    {"relative_to": "dir", "iter_entries": []})
add("build_folder_listing", {"path": "/w/dir", "cwd": "/w", "display_base": "/other"},
    {"relative_to": [R("ValueError", "not relative"), "dir"],
     "iter_entries": [{"path": "/w/dir/a.py", "name": "a.py", "is_dir": False, "relparts": 2}],
     "file_metadata": "5 lines"})
add("build_folder_listing", {"path": "/w/dir", "cwd": "/w"},
    {"relative_to": "dir",
     "iter_entries": [{"path": "/w/dir/sub", "name": "sub", "is_dir": True, "relparts": 2},
                      {"path": "/w/dir/sub/deep.py", "name": "deep.py", "is_dir": False, "relparts": 3}],
     "file_metadata": "9 lines"})
add("build_folder_listing", {"path": "/x/dir", "cwd": "/w", "display_base": "/x", "limit": 2},
    {"relative_to": [R("ValueError", "a"), R("ValueError", "b")],
     "iter_entries": [{"path": "/x/dir/a.py", "name": "a.py", "is_dir": False, "relparts": 2},
                      {"path": "/x/dir/b.py", "name": "b.py", "is_dir": False, "relparts": 2}],
     "file_metadata": ["1 lines", "2 lines"]})
add("build_folder_listing", {"path": "/w/dir", "cwd": "/w"},
    {"relative_to": R("SystemExit", "bye")})
add("build_folder_listing", {"path": "/w/dir", "cwd": "/w"})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()
