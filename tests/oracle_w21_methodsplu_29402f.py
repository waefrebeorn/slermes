#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_plugin_catalog.py
(gate w21_methodsplu_29402f).

The live handler is imported from its real source with the registry plumbing
stubbed (`tui_gateway.method_ctx.HandlerRegistry` / `bind_module`) and `_ok`
bound as server.py's `bind_module` would bind it. Its two deferred imports are
also live code lifted out of the real files:
  * hermes_cli.plugin_catalog: match_removed, _normalize_repo, _NAME_RE, _SCP_URL_RE
  * hermes_cli.plugins_cmd_catalog: looks_like_catalog_name
Only the catalog reader and the kill-list resolver are seams: the fixture's
entries/removed lists stand in for `load_catalog_live()` and
`resolved_removed_entries()` (network + cache + in-tree fallback).
"""
import __future__
import ast
import importlib.util
import json
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
GATE = "w21_methodsplu_29402f"
SRC = QUARRY + "/tui_gateway/methods_plugin_catalog.py"
CATALOG = QUARRY + "/hermes_cli/plugin_catalog.py"
CMD_CATALOG = QUARRY + "/hermes_cli/plugins_cmd_catalog.py"
_ANN = __future__.annotations.compiler_flag


def _exec(path, names, into):
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                         flags=_ANN, dont_inherit=True), into)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in names for t in targets):
                exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                             flags=_ANN, dont_inherit=True), into)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# hermes_cli.plugin_catalog: the kill-list matcher and its normalisation
CAT = {"re": re}
_exec(CATALOG, {"match_removed", "_normalize_repo", "_NAME_RE", "_SCP_URL_RE"}, CAT)
_mk("hermes_cli")
_mk("hermes_cli.plugin_catalog", match_removed=CAT["match_removed"],
    _normalize_repo=CAT["_normalize_repo"], _NAME_RE=CAT["_NAME_RE"],
    _SCP_URL_RE=CAT["_SCP_URL_RE"],
    load_catalog_live=lambda: list(ENTRIES), resolved_removed_entries=lambda: list(REMOVED))

# hermes_cli.plugins_cmd / plugins_cmd_catalog: the name rule
CMD = {"re": re, "_NAME_RE": CAT["_NAME_RE"],
       "_URL_SCHEMES": ("https://", "http://", "git@", "ssh://", "file://")}
_exec(CMD_CATALOG, {"looks_like_catalog_name"}, CMD)
_mk("hermes_cli.plugins_cmd", _URL_SCHEMES=CMD["_URL_SCHEMES"])
_mk("hermes_cli.plugins_cmd_catalog",
    looks_like_catalog_name=CMD["looks_like_catalog_name"])

# tui_gateway.method_ctx: the registry plumbing (decorators return the handler)
class HandlerRegistry:
    def method(self, name):
        def deco(fn):
            return fn
        return deco

    def profile_scoped(self, fn):
        return fn


_mk("tui_gateway")
_mk("tui_gateway.method_ctx", HandlerRegistry=HandlerRegistry,
    bind_module=lambda *a, **k: None)

ENTRIES = []
REMOVED = []

_sp = importlib.util.spec_from_file_location("tui_gateway.methods_plugin_catalog", SRC)
live = importlib.util.module_from_spec(_sp)
sys.modules["tui_gateway.methods_plugin_catalog"] = live
_sp.loader.exec_module(live)
live._ok = lambda rid, payload: payload  # bound from server.py globals upstream


class Entry:
    def __init__(self, d):
        self.name = d.get("name", "")
        self.repo = d.get("repo", "")
        self.sha = d.get("sha", "")
        self.subdir = d.get("subdir", "")
        self.tier = d.get("tier", "")

    def __repr__(self):
        return "Entry(%r)" % (self.name,)


class Removed:
    def __init__(self, d):
        self.name = d.get("name", "")
        self.repo = d.get("repo", "")
        self.reason = d.get("reason", "")
        self.date = d.get("date", "")


def expected(r):
    global ENTRIES, REMOVED
    f = r.get("func")
    if f == "lookup":
        ENTRIES = [Entry(e) for e in (r.get("entries") or [])]
        REMOVED = [Removed(e) for e in (r.get("removed") or [])]
        return live._("rid-1", r.get("params") or {})
    if f == "looks_like":
        return CMD["looks_like_catalog_name"](r.get("name"))
    if f == "normalize_repo":
        return CAT["_normalize_repo"](r.get("url"))
    if f == "match_removed":
        entries = [Removed(e) for e in (r.get("removed") or [])]
        return CAT["match_removed"](r.get("candidate"), entries) is not None
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"lookup", "looks_like", "normalize_repo", "match_removed"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
