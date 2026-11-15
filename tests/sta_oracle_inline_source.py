#!/usr/bin/env python3
"""sta_oracle_inline_source.py -- diffs the C status_inline_source.py port.

Reads the harness JSONL on stdin. The Python reference is extracted with ast;
the two option tables are supplied directly (the function imports them from
hermes_state_holders at call time, which is not importable here).
"""
import ast
import json
import shlex
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/gateway/status_inline_source.py"

# inline_source_flag_index imports these at CALL time from hermes_state_holders,
# which is not importable here. Values verbatim from that module:39-42.
_sh = types.ModuleType("hermes_state_holders")
_sh._PYTHON_SHORT_OPTIONS_WITH_OPERANDS = frozenset({"Q", "W", "X"})
_sh._PYTHON_LONG_OPTIONS_WITH_OPERANDS = frozenset(
    {"--check-hash-based-pycs", "--jit"})
sys.modules.setdefault("hermes_state_holders", _sh)
with open(SRC, encoding="utf-8") as f:
    text = f.read()
tree = ast.parse(text)

WANTED = {"inline_source_flag_index"}
ns = {
    "re": __import__("re"),
    "Optional": object,
    "Any": object,
    "__name__": "oracle_inline_source_extracted",
    # verbatim from hermes_state_holders.py:39-42
    "_PYTHON_SHORT_OPTIONS_WITH_OPERANDS": frozenset({"Q", "W", "X"}),
    "_PYTHON_LONG_OPTIONS_WITH_OPERANDS": frozenset(
        {"--check-hash-based-pycs", "--jit"}),
}

picked = set()
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
        picked.add(node.name)

missing = (WANTED | {"_PYTHON_LONG_OPTIONS_WITH_OPERANDS",
                     "_PYTHON_SHORT_OPTIONS_WITH_OPERANDS"}) - picked
# the two tables are supplied above, not extracted
missing -= {"_PYTHON_LONG_OPTIONS_WITH_OPERANDS",
            "_PYTHON_SHORT_OPTIONS_WITH_OPERANDS"}
if missing:
    print(f"ORACLE inline_source: FATAL, source not found: {sorted(missing)}")
    raise SystemExit(1)

cases = mismatches = shown = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    cases += 1
    cmd = c.get("cmd", "")
    try:
        tokens = shlex.split(cmd, posix=False)
    except ValueError:
        tokens = cmd.split()
    want = ns["inline_source_flag_index"](tokens)
    if want is None:
        want = -1  # the C port's not-found sentinel
    got = c["out"]
    if isinstance(want, bool):
        got = bool(got)
    if got != want:
        mismatches += 1
        if shown < 12:
            print(f"MISMATCH flag cmd={cmd!r} tok={tokens!r}: "
                  f"got {got!r} want {want!r}")
            shown += 1

print(f"ORACLE inline_source: {cases} cases, {mismatches} mismatches")
raise SystemExit(1 if mismatches else 0)
