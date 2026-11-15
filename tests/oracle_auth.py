#!/usr/bin/env python3
"""oracle_auth.py -- diffs the C hermes_cli/auth.py port against the real source.

The Python reference is ast-extracted from auth.py (which is not importable: it
pulls pm, agent.credential_persistence, webbrowser, threading, ...). urlparse is
supplied, the placeholder/prefix tables are extracted from the module body, and
`logger` is a stub whose warning() records the call.
"""
import ast
import json
import sys
import types
from urllib.parse import urlparse

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth.py"
with open(SRC, encoding="utf-8") as f:
    text = f.read()
tree = ast.parse(text)

WANTED = {
    "_nonempty_str", "_is_placeholder_shape", "has_usable_secret",
    "_matches_key_prefix", "looks_like_openrouter_key",
    "_usable_declared_secret", "is_actual_local_base_url",
    "normalize_actual_base_url",
}
CONSTS = {"_PLACEHOLDER_SECRET_VALUES", "_PLACEHOLDER_KEY_PREFIXES",
          "KNOWN_PROVIDER_KEY_PREFIXES"}

ns = {
    "urlparse": urlparse,
    "Optional": object,
    "Any": object,
    "str": str,
    "bool": bool,
    "int": int,
    "len": len,
    "all": all,
    "set": set,
    "__name__": "oracle_auth_extracted",
}

# logger.warning seam
_logger = types.SimpleNamespace()
_logger.warnings = 0


def _warning(*_a, **_k):
    _logger.warnings += 1


_logger.warning = _warning
ns["logger"] = _logger

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
    elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
        if node.target.id in CONSTS and node.value is not None:
            exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
            picked.add(node.target.id)

missing = (WANTED | CONSTS) - picked
if missing:
    print(f"ORACLE auth: FATAL, source not found: {sorted(missing)}")
    raise SystemExit(1)

DEFAULT_ACTUAL = "https://api.actual.inc/v1"
ns["DEFAULT_ACTUAL_BASE_URL"] = DEFAULT_ACTUAL

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

    if fn == "nonempty":
        want = ns["_nonempty_str"](arg)
    elif fn == "placeholder":
        want = ns["_is_placeholder_shape"](arg)
    elif fn == "usable":
        want = ns["has_usable_secret"](arg, min_length=c.get("min", 4))
    elif fn == "prefix":
        want = ns["_matches_key_prefix"](c.get("prov", ""), arg)
    elif fn == "openrouter":
        want = ns["looks_like_openrouter_key"](arg)
    elif fn == "declared":
        _logger.warnings = 0
        want_val = ns["_usable_declared_secret"](c.get("prov", ""), arg, ".env")
        want = {"ok": want_val is not None,
                "warned": bool(_logger.warnings),
                "out": want_val if want_val is not None else ""}
    elif fn == "local":
        want = ns["is_actual_local_base_url"](arg)
    elif fn == "normalize":
        want = ns["normalize_actual_base_url"](arg)
    else:
        continue

    got = c["out"]
    if isinstance(got, str) and got[:1] in ("[", "{"):
        try:
            got = json.loads(got)
        except json.JSONDecodeError:
            pass
    if fn == "declared":
        got = {"ok": bool(c.get("ok")), "warned": bool(c.get("warned")),
               "out": c.get("out")}
    if isinstance(want, bool):
        got = bool(got)
    if got != want:
        mismatches += 1
        if shown < 12:
            print(f"MISMATCH {fn} {arg!r}: got {got!r} want {want!r}")
            shown += 1

print(f"ORACLE auth: {cases} cases, {mismatches} mismatches")
raise SystemExit(1 if mismatches else 0)
