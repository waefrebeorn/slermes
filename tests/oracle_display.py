#!/usr/bin/env python3
"""oracle_display.py -- diffs the C agent/display.py port against the real source.

Reads the harness JSONL on stdin. The Python reference is ast-extracted, with the
transitive helper (_scan_quoted) and the two shell word-sets included. The `t()`
call at the tail of summarize_shell_command is stubbed with the real en.yaml
templates, which the harness also passes verbatim.
"""
import ast
import json
import re
import sys

SRC = "C:/Users/eman5/hermes-agent/agent/display.py"
with open(SRC, encoding="utf-8") as f:
    text = f.read()
tree = ast.parse(text)

WANTED = {
    "_oneline", "_tail_trunc", "_clip", "_shell_basename", "_scan_quoted",
    "_split_shell_words", "_strip_shell_pipe_tail", "_split_shell_compound",
    "_shell_head_word", "_clean_shell_segment", "_is_shell_boundary_echo",
    "summarize_shell_command",
}
CONSTS = {"_SHELL_SILENT_HEADS", "_SHELL_PIPE_TAIL_HEADS"}

ns = {
    "re": re,
    "Iterator": object,
    "tuple": tuple,
    "str": str,
    "bool": bool,
    "int": int,
    "list": list,
    "__name__": "oracle_display_extracted",
}

picked = set()
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
        picked.add(node.name)
    elif isinstance(node, ast.Assign):
        names = {t.id for t in node.targets if isinstance(t, ast.Name)}
        if names & CONSTS:
            exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
            picked |= (names & CONSTS)

missing = (WANTED | CONSTS) - picked
if missing:
    print(f"ORACLE display: FATAL, source not found: {sorted(missing)}")
    raise SystemExit(1)

# The i18n seam, verbatim from locales/en.yaml:3506-3507.
PLUS_ONE = "{first} + {count} command"
PLUS_MANY = "{first} + {count} commands"
ns["t"] = lambda key, **kw: (PLUS_ONE if key.endswith("shell_plus_one")
                             else PLUS_MANY).format(**kw)

cases = mismatches = shown = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    fn = c["fn"]
    arg = c.get("arg", "")
    cases += 1

    if fn == "oneline":
        want = ns["_oneline"](arg)
    elif fn == "tail":
        want = ns["_tail_trunc"](arg, c.get("lim", 0))
    elif fn == "clip":
        want = ns["_clip"](arg, c.get("n", 0))
    elif fn == "words":
        want = ns["_split_shell_words"](arg)
    elif fn == "pipetail":
        want = ns["_strip_shell_pipe_tail"](arg)
    elif fn == "compound":
        want = ns["_split_shell_compound"](arg)
    elif fn == "head":
        want = ns["_shell_head_word"](arg)
    elif fn == "clean":
        want = ns["_clean_shell_segment"](arg)
    elif fn == "echo":
        want = bool(ns["_is_shell_boundary_echo"](arg))
    elif fn == "summary":
        want = ns["summarize_shell_command"](arg)
    else:
        continue

    got = c["out"]
    # The harness emits `out` as a JSON string holding serialised JSON. Unwrap it
    # ONLY for structured values: json.loads("true") is the bool True, so a
    # plain string that happens to read "true" must stay a string.
    if isinstance(got, str) and got[:1] in ("[", "{"):
        try:
            got = json.loads(got)
        except json.JSONDecodeError:
            pass
    if isinstance(want, bool):
        got = bool(got)
    if got != want:
        mismatches += 1
        if shown < 12:
            print(f"MISMATCH {fn} {arg!r}: got {got!r} want {want!r}")
            shown += 1

print(f"ORACLE display: {cases} cases, {mismatches} mismatches")
raise SystemExit(1 if mismatches else 0)
