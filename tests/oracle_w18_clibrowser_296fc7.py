#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_browser_gateway.py (gate w18_clibrowser_296fc7).

The live defs are AST-extracted: importing the module pulls in the gateway client
and session-tools chains.  We still put the quarry on sys.path, register the live
module in sys.modules and bootstrap a parent `hermes_cli` package so any sibling
import the defs attempt resolves against the real package.
"""
import ast
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

# parent-package bootstrap: register a usable `hermes_cli` package in sys.modules
_pkg_name = "hermes_cli"
if _pkg_name not in sys.modules:
    try:
        import hermes_cli as _pkg  # noqa: F401
    except Exception:
        _pkg = types.ModuleType(_pkg_name)
        _pkg.__path__ = [os.path.join(QUARRY, "hermes_cli")]
        sys.modules[_pkg_name] = _pkg

SRC = os.path.join(QUARRY, "hermes_cli", "cli_browser_gateway.py")
_want = ("_gateway_is_remote", "_normalize_cdp_url")
_src = open(SRC, encoding="utf-8").read()
_tree = ast.parse(_src)
_ns = {
    "os": os,
    "urlparse": __import__("urllib.parse", fromlist=["urlparse"]).urlparse,
    "_say_block": lambda *a, **k: None,
    "_t": lambda key, **k: key,
    "DEFAULT_BROWSER_CDP_URL": "http://localhost:9222",
}
for _node in _tree.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in _want:
        exec(compile(ast.Module(body=[_node], type_ignores=[]), SRC, "exec"), _ns)

live = types.ModuleType("w18_clibrowser_296fc7_live")
for _k in _want:
    setattr(live, _k, _ns[_k])
sys.modules["w18_clibrowser_296fc7_live"] = live


def compute(r):
    func = r.get("func")
    if func == "gateway_is_remote":
        os.environ["HERMES_TUI_GATEWAY_URL"] = str(r.get("env_value", ""))
        return bool(live._gateway_is_remote())
    if func == "normalize_cdp_url":
        res = live._normalize_cdp_url(str(r.get("cdp_url", "")))
        if res is None:
            return None
        return [res[0], res[1]]
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
print(f"ORACLE w18_clibrowser_296fc7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
