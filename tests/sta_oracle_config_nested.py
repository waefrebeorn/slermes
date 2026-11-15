#!/usr/bin/env python3
"""sta_oracle_config_nested.py -- diffs the C nested-config port against Python.

Reads the harness's JSONL on stdin and compares each result with the real
hermes_cli/config.py source, extracted by ast (the module itself cannot be
imported here: ruamel and friends are absent).
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

WANTED = {"_split_key_path", "_greedy_literal_match", "_phantom_sibling",
          "_set_nested", "_locate_nested", "_get_nested", "_unset_nested"}

_MISSING_SENTINEL = object()
ns = {"re": re, "platform": platform, "Any": object, "Optional": object,
      "_MISSING": _MISSING_SENTINEL,
      "__name__": "oracle_config_nested_extracted"}
_stub = types.ModuleType("hermes_cli.auth_external_process")
_stub.env_writer_launch_denylist = lambda: frozenset()
sys.modules.setdefault("hermes_cli", types.ModuleType("hermes_cli"))
sys.modules["hermes_cli.auth_external_process"] = _stub

picked = set()
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
        picked.add(node.name)
missing = WANTED - picked
if missing:
    print(f"ORACLE config_nested: FATAL, source not found: {sorted(missing)}")
    raise SystemExit(1)

_MISSING = object()


def run(tree_obj, op, key, value):
    """Mirror the harness: returns (rc, out) where out is JSON-serialisable."""
    if op == "set":
        try:
            ns["_set_nested"](tree_obj, key, value)
            return 0, tree_obj
        except ValueError:
            return 1, tree_obj          # phantom-sibling refusal
        except TypeError:
            return 2, tree_obj          # un-navigable type
        except IndexError:
            return 2, tree_obj          # list assignment out of range
    if op == "get":
        v = ns["_get_nested"](tree_obj, key)
        return 0, ("MISSING" if v is _MISSING_SENTINEL else v)
    if op == "unset":
        # Python returns a bool; the C returns 1/0.
        return int(ns["_unset_nested"](tree_obj, key)), tree_obj
    if op == "locate":
        parts = ns["_split_key_path"](key)
        loc = ns["_locate_nested"](tree_obj, parts)
        return (1 if loc else 0), (loc[2] if loc else None)
    raise ValueError(op)


cases = mismatches = shown = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    c = json.loads(line)
    cases += 1
    try:
        tree_obj = json.loads(c["tree"])
    except Exception:
        continue
    raw_value = c.get("value")
    value = json.loads(raw_value) if raw_value else None

    try:
        want_rc, want_out = run(tree_obj, c["op"], c["key"], value)
    except Exception as e:
        print(f"MISMATCH {c['op']} {c['key']!r}: oracle raised {e!r}")
        mismatches += 1
        continue

    # Normalise both sides to JSON text for comparison.
    # The harness emits `out` as a JSON string holding serialised JSON (or the
    # bare token MISSING). Parse it back so both sides compare as objects.
    got_raw = c["out"]
    if isinstance(got_raw, str) and got_raw != "MISSING":
        try:
            got_raw = json.loads(got_raw)
        except json.JSONDecodeError:
            pass
    want = json.dumps(want_out, sort_keys=True)
    got = json.dumps(got_raw, sort_keys=True)
    if c["rc"] != want_rc or got != want:
        mismatches += 1
        if shown < 12:
            print(f"MISMATCH {c['op']} {c['key']!r} tree={c['tree']}: "
                  f"got rc={c['rc']} out={c['out']!r}  want rc={want_rc} out={want_out!r}")
            shown += 1

print(f"ORACLE config_nested: {cases} cases, {mismatches} mismatches")
raise SystemExit(1 if mismatches else 0)
