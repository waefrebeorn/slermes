#!/usr/bin/env python3
"""Generate tests/fixtures/context_references_flow2.jsonl — >=5 cases per ported
function (10 funcs), covering decision branches, error paths, the FULL builtin
exception hierarchy (Exception children fold, OSError grandchildren fold,
BaseException trio propagates), real composition and fail-closed missing seams.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script is a
RuntimeError raise spec; an array is a per-call queue (last repeats); a single
value is a constant; a scripted null is None; {"__raise__":cls,"msg":...} is an
exception (built as a REAL builtin class so `except` binds by hierarchy identity).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "context_references_flow2.jsonl")

CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def add(func, inp=None, seams=None, state=None):
    d = dict(inp or {})
    d["seams"] = seams or {}
    if state is not None:
        d["state"] = state
    CASES.append({"func": func, "in": d})


# ── format_reference_value ──
for v in ["plain", "simple_path.py", "with space", "a(b)", "x[y]", "a{b}",
          "a<b>", 'say "hi"', "it's", "`tick`", "`both'\"`", ""]:
    add("format_reference_value", {"value": v})

# ── _strip_trailing_punctuation ──
for v in ["hello,", "world.", "abc?!;", "(see x)", "(see x))", "[a]",
          "text}", "no punctuation", "a)!", "((x))"]:
    add("strip_trailing_punctuation", {"value": v})

# ── _strip_reference_wrappers ──
for v in ["`quoted`", '"dq"', "'sq'", "bare", "``", "a`", "`a", "", "x'y'"]:
    add("strip_reference_wrappers", {"value": v})

# ── _parse_file_reference_value ──
for v in ["file.py", "`file.py`", '"my file.py"', "file.py:10", "file.py:10-20",
          "`a b.py`:5-9", "dir/file.py:3", "", "plain", "'quoted.md':7"]:
    add("parse_file_reference_value", {"value": v})

# ── get_context_reference_providers ──
add("get_providers", {"providers": {}})
add("get_providers", {"providers": {"issue": {"prefix": "issue"}}})
add("get_providers", {"providers": {"a": {"x": 1}, "b": {"y": 2}}})
add("get_providers", {"providers": {"nil": None}})
add("get_providers", {})

# ── _is_binary_file ──
add("is_binary_file", {"name": "a.png", "path": "/w/a.png"},
    {"guess_type": "image/png", "sniff": [137, 80, 78, 71]})
add("is_binary_file", {"name": "a.png", "path": "/w/a.png"},
    {"guess_type": "image/png", "sniff": [1, 0, 3]})
add("is_binary_file", {"name": "a.py", "path": "/w/a.py"},
    {"guess_type": "text/x-python", "sniff": [0, 10]})
add("is_binary_file", {"name": "weird.pyc", "path": "/w/weird.pyc"},
    {"guess_type": "application/octet-stream", "sniff": [1, 2, 3]})
add("is_binary_file", {"name": "a.bin", "path": "/w/a.bin"},
    {"guess_type": R("SystemExit", "bye"), "sniff": [1, 2]})
add("is_binary_file", {"name": "a.bin", "path": "/w/a.bin"})  # fail-closed

# ── _agent_visible_path (except Exception -> fallback; BaseException propagates) ──
add("agent_visible_path", {"path": "/h/x"})
add("agent_visible_path", {"path": "/h/x"},
    {"ensure_bridged": None, "to_cache": "/mnt/cache/x"})
add("agent_visible_path", {"path": "/h/x"},
    {"ensure_bridged": None, "to_cache": R("RuntimeError", "no backend")})
add("agent_visible_path", {"path": "/h/x"},
    {"ensure_bridged": None, "to_cache": R("ValueError", "bad")})
add("agent_visible_path", {"path": "/h/x"},
    {"ensure_bridged": None, "to_cache": R("SystemExit", "bye")})
add("agent_visible_path", {"path": "/h/x"},
    {"ensure_bridged": None, "to_cache": R("KeyboardInterrupt", "stop")})
add("agent_visible_path", {"path": "/h/x"},
    {"ensure_bridged": None, "to_cache": R("GeneratorExit", "g")})

# ── _binary_reference_block ──
BLOCK_ARGS = {
    "reason": "binary file, not inlined as text.",
    "guidance": "Use your tools to work with it (read or convert it, extract its text, "
                "or view/render it as needed); do not tell the user the file type is unsupported.",
}
add("binary_reference_block", {"raw": "@file:img.png", "name": "img.png"},
    {"guess_type": "image/png", "on_disk": "BLOCK png"})
add("binary_reference_block", {"raw": "@file:x.unknown", "name": "x.unknown"},
    {"guess_type": None, "on_disk": "BLOCK octet"})
add("binary_reference_block", {"raw": "@file:a.bin", "name": "a.bin"},
    {"guess_type": "application/octet-stream", "on_disk": R("ValueError", "stat fail")})
add("binary_reference_block", {"raw": "@file:a.bin", "name": "a.bin"},
    {"guess_type": R("SystemExit", "bye"), "on_disk": "never"})
add("binary_reference_block", {"raw": "@file:a.bin", "name": "a.bin"})  # fail-closed

# ── _file_metadata (except OSError / (LiveConnectionError, OSError)) ──
add("file_metadata", {"path": "/w/f.txt"},
    {"stat": {"size": 42}, "cm_enter": None, "is_binary": False,
     "read": ["hello\n", "world\n"]})
add("file_metadata", {"path": "/w/f.txt"},
    {"stat": {"size": 42}, "cm_enter": None, "is_binary": False,
     "read": "one line"})
add("file_metadata", {"path": "/w/f.txt"},
    {"stat": {"size": 42}, "cm_enter": None, "is_binary": True, "read": ""})
# size > 4 MiB
add("file_metadata", {"path": "/w/big.txt"},
    {"stat": {"size": 5000000}, "cm_enter": None, "is_binary": False, "read": ""})
# OSError family folding on stat
add("file_metadata", {"path": "/w/no.txt"},
    {"stat": R("FileNotFoundError", "gone")})
add("file_metadata", {"path": "/w/no.txt"},
    {"stat": R("PermissionError", "denied")})
# BaseException trio propagates on stat
add("file_metadata", {"path": "/w/no.txt"},
    {"stat": R("SystemExit", "bye")})
# LiveConnectionError folds in the with-block; RuntimeError does NOT
add("file_metadata", {"path": "/w/live.db"},
    {"stat": {"size": 7}, "cm_enter": R("LiveConnectionError", "live")})
add("file_metadata", {"path": "/w/x.txt"},
    {"stat": {"size": 7}, "cm_enter": R("RuntimeError", "unexpected")})
# nested OSError child folds on is_binary; SystemExit propagates
add("file_metadata", {"path": "/w/x.txt"},
    {"stat": {"size": 7}, "cm_enter": None, "is_binary": R("ConnectionResetError", "reset")})
add("file_metadata", {"path": "/w/x.txt"},
    {"stat": {"size": 7}, "cm_enter": None, "is_binary": R("UnicodeDecodeError", "bad")})
# read raises a foldable OSError child
add("file_metadata", {"path": "/w/x.txt"},
    {"stat": {"size": 7}, "cm_enter": None, "is_binary": False,
     "read": R("NotADirectoryError", "nope")})
# fail-closed at stat
add("file_metadata", {"path": "/w/x.txt"})

# ── register_context_reference_provider ──
def st(providers=None, owners=None):
    return {"providers": providers or {}, "owners": owners or {}}


add("register_provider", {"prefix": "issue", "is_provider": True, "owner": "p1"}, state=st())
add("register_provider", {"prefix": "  ISSUE  ", "is_provider": True, "owner": "p1"}, state=st())
add("register_provider", {"prefix": "diff", "is_provider": True, "owner": "p1"}, state=st())
add("register_provider", {"prefix": "", "is_provider": True, "owner": "p1"}, state=st())
add("register_provider", {"prefix": "issue", "is_provider": False, "owner": "p1"}, state=st())
add("register_provider", {"prefix": "issue", "is_provider": True, "owner": "p1"},
    state=st({"issue": {"id": 1}}, {"issue": "p1"}))
add("register_provider", {"prefix": "issue", "is_provider": True, "owner": "p2", "reclaim": True},
    state=st({"issue": {"id": 1}}, {"issue": "p1"}))
add("register_provider", {"prefix": "issue", "is_provider": True, "owner": "p1", "reclaim": True},
    state=st({"issue": {"id": 1}}, {"issue": "p1"}))
add("register_provider", {"prefix": "issue", "is_provider": True, "owner": "p1",
                          "reclaim": True, "publish": True},
    state=st({"issue": {"id": 1}}, {"issue": "p1"}))
add("register_provider", {"prefix": "issue", "is_provider": True, "owner": "p1",
                          "reclaim": True, "publish": True, "publish_changed": True},
    state=st({"issue": {"id": 1}}, {"issue": "p1"}))


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
