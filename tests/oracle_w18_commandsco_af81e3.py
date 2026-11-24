#!/usr/bin/env python3
"""Differential oracle for hermes_cli/commands_completion.py (gate w18_commandsco_af81e3)."""
import ast
import functools
import json
import os
import sys
import types

for _s in ("stdin", "stdout", "stderr"):
    try:
        getattr(sys, _s).reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
_pkg = "hermes_cli"
if _pkg not in sys.modules:
    try:
        import hermes_cli  # noqa: F401
    except Exception:
        _p = types.ModuleType(_pkg); _p.__path__ = [os.path.join(QUARRY, "hermes_cli")]
        sys.modules[_pkg] = _p

SRC = os.path.join(QUARRY, "hermes_cli", "commands_completion.py")
_tree = ast.parse(open(SRC, encoding="utf-8").read())
_ns = {"os": os, "functools": functools, "Completer": object, "AutoSuggest": object}


def _strip(node):
    for n in ast.walk(node):
        if isinstance(n, ast.AnnAssign):
            continue
        if hasattr(n, "annotation"):
            n.annotation = None
        if hasattr(n, "returns"):
            n.returns = None
    return node


for _node in _tree.body:
    if isinstance(_node, (ast.FunctionDef, ast.ClassDef)):
        _node = _strip(_node)
        try:
            exec(compile(ast.Module(body=[_node], type_ignores=[]), SRC, "exec"), _ns)
        except Exception:
            pass

cls = _ns["SlashCommandCompleter"]


def compute(r):
    fn = r.get("func")
    if fn == "split_args":
        completed, partial = _ns["_split_args"](r.get("sub_text", ""))
        return [completed, partial]
    if fn == "extract_path_word":
        return _ns["_extract_path_word"](r.get("text", ""))
    if fn == "score_path":
        return _ns["_score_path"](r.get("filepath", ""), r.get("query", ""))
    if fn == "normalize_skill_token":
        return cls._normalize_skill_token(r.get("token", ""))
    if fn == "completion_text":
        return cls._completion_text(r.get("cmd_name", ""), r.get("word", ""))
    if fn == "extract_context_word":
        return cls._extract_context_word(r.get("text", ""))
    return "__UNKNOWN__"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    if py == "__UNKNOWN__":
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_commandsco_af81e3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
