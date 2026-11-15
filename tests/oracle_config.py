#!/usr/bin/env python3
"""oracle_config.py -- diffs the C port of hermes_cli/config.py against Python.

Reads the harness's JSONL on stdin (one object per case, carrying the C result)
and compares each `out` against the real Python source.

The module itself pulls in dependencies (ruamel, auth_external_process) that are
not installed here, so the exact source of each needed function and constant is
extracted with ast and exec'd in a namespace. The behaviour under test is still
the verbatim Python source text.
"""
import ast
import json
import platform
import re
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/config.py"

with open(SRC, encoding="utf-8") as f:
    text = f.read()
tree = ast.parse(text)

WANTED_FUNCS = {
    "_split_key_path",
    "_env_var_policy_name",
    "validate_env_var_name_for_write",
    "_is_env_config_key",
}
WANTED_CONSTS = {
    "_IS_WINDOWS",
    "_ENV_VAR_NAME_RE",
    "_ENV_VAR_NAME_DENY_PREFIXES",
    "_ENV_VAR_NAME_DENYLIST",
    "_ENV_CONFIG_KEYS",
}

ns = {"re": re, "platform": platform, "Optional": object,
      "__name__": "oracle_config_extracted"}

# validate_env_var_name_for_write also consults
# hermes_cli.auth_external_process.env_writer_launch_denylist(), which is not
# importable here. The C port deliberately does not model that external list
# either, so both sides run with it empty -- the comparison covers the built-in
# name shape, denylist and prefixes, which is what was ported.
_stub_mod = types.ModuleType("hermes_cli.auth_external_process")
_stub_mod.env_writer_launch_denylist = lambda: frozenset()
sys.modules.setdefault("hermes_cli", types.ModuleType("hermes_cli"))
sys.modules["hermes_cli.auth_external_process"] = _stub_mod

picked = set()
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED_FUNCS:
        exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
        picked.add(node.name)
    elif isinstance(node, ast.Assign):
        names = {t.id for t in node.targets if isinstance(t, ast.Name)}
        if names & WANTED_CONSTS:
            exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
            picked |= (names & WANTED_CONSTS)
    elif isinstance(node, ast.AnnAssign):
        # `_X: tuple[str, ...] = (...)` -- annotated assignments are a distinct
        # node type, so the Assign branch missed both denylist constants.
        if isinstance(node.target, ast.Name) and node.target.id in WANTED_CONSTS:
            exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
            picked.add(node.target.id)

missing = (WANTED_FUNCS | WANTED_CONSTS) - picked
if missing:
    print(f"ORACLE config: FATAL, source not found: {sorted(missing)}")
    raise SystemExit(1)


def py_split(key):
    return ns["_split_key_path"](key)


def py_policy(key, win):
    return ns["_env_var_policy_name"](key, is_windows=win)


def py_envkey(key):
    return 1 if ns["_is_env_config_key"](key) else 0


def py_validate(key, win):
    try:
        ns["validate_env_var_name_for_write"](key)
        return 0
    except ValueError as e:
        return 1 if "Invalid environment variable name" in str(e) else 2


cases = mismatches = 0
shown = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    fn, arg, win = c["fn"], c["arg"], c.get("win", 0)
    got = c["out"]
    cases += 1

    if fn == "split":
        want = py_split(arg)
    elif fn == "policy":
        want = py_policy(arg, win)
    elif fn == "envkey":
        want = py_envkey(arg)
    elif fn == "validate":
        want = py_validate(arg, win)
    else:
        continue

    if got != want:
        mismatches += 1
        if shown < 12:
            print(f"MISMATCH {fn} arg={arg!r} win={win}: got {got!r} want {want!r}")
            shown += 1

print(f"ORACLE config: {cases} cases, {mismatches} mismatches")
raise SystemExit(1 if mismatches else 0)
