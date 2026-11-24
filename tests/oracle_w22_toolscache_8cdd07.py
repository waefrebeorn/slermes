#!/usr/bin/env python3
"""Differential oracle for tools/connectors/portal/tools_cache.py
(gate w22_toolscache_8cdd07).

Network, cache directory and clock are seams; the live logic runs:

  member_key  -- live `_member_key` (real base64/hashlib/json).
  slug_valid  -- live `validate_slug` + `_SLUG_RE` lifted from client.py with the
                 live exception class standing in for InvalidConnectorSlug.
  cache_rel   -- live `_cache_path` with get_hermes_home bound to a fixed home.
  read_plan   -- live `read_tools`, with the client object, `_cached`, `_store`
                 and the cache dir stubbed (network/filesystem/clock seams).
"""
import ast
import base64
import binascii
import dataclasses
import hashlib
import json
import os
import re
import sys
import time
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tools" / "connectors" / "portal" / "tools_cache.py"
CLIENT = QUARRY / "tools" / "connectors" / "portal" / "client.py"
GATE = "w22_toolscache_8cdd07"


class InvalidConnectorSlug(Exception):
    pass


class GatewayUnavailable(Exception):
    pass


class GatewayAuthError(Exception):
    pass


class PortalToolsUnavailable(Exception):
    def __init__(self, message, code=None):
        super().__init__(message)
        self.code = code


class ToolGatewayError(Exception):
    pass


class NotModified:
    pass


@dataclasses.dataclass(frozen=True)
class ToolsRead:
    connector: str
    toolkit_version: str
    etag: str
    fetched_at: float
    source: str
    stale: bool
    tools: list


def lift(path, funcs, assigns, globs):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    nodes = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in funcs:
            nodes.append((node.lineno, node))
        elif isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in assigns for t in node.targets):
            nodes.append((node.lineno, node))
    for _, node in sorted(nodes, key=lambda p: p[0]):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            node.decorator_list = []
        exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec"), globs)
    return globs


_MK = lift(SRC, {"_member_key"}, set(),
           {"base64": base64, "binascii": binascii, "hashlib": hashlib, "json": json})
member_key_live = _MK["_member_key"]

_SLUG = lift(CLIENT, {"validate_slug"}, {"_SLUG_RE"},
             {"re": re, "InvalidConnectorSlug": InvalidConnectorSlug})
validate_slug_live = _SLUG["validate_slug"]

_CACHE = lift(SRC, {"_cache_path"}, set(),
              {"hashlib": hashlib, "get_hermes_home": lambda: Path("/H")})
cache_path_live = _CACHE["_cache_path"]

_TREE = ast.parse(SRC.read_text(encoding="utf-8"))
READ_TOOLS_NODE = None
for _n in ast.walk(_TREE):
    if isinstance(_n, (ast.FunctionDef, ast.AsyncFunctionDef)) and _n.name == "read_tools":
        _n.decorator_list = []
        READ_TOOLS_NODE = _n
        break
if READ_TOOLS_NODE is None:  # pragma: no cover
    raise SystemExit("read_tools not found in %s" % SRC)

WANTED = {"member_key", "slug_valid", "cache_rel", "home_env", "read_plan"}


def cache_rel(origin, key, slug):
    p = cache_path_live(origin, key, slug)
    return p.relative_to(Path("/H")).as_posix()


def read_plan(r):
    state = {"stored": False, "unlinked": False}
    raw = r.get("cached")
    cached = None
    if isinstance(raw, dict) and all(
            k in raw for k in ("connector", "toolkit_version", "etag", "fetched_at", "tools")):
        try:
            cached = ToolsRead(str(raw["connector"]), str(raw["toolkit_version"]),
                               str(raw["etag"]), float(raw["fetched_at"]), "cache", False,
                               list(range(int(raw["tools"]))))
        except (TypeError, ValueError):
            cached = None

    class FakePath:
        def unlink(self, missing_ok=False):
            state["unlinked"] = True

    def store(_path, entry):
        state["stored"] = True
        return entry

    class Client:
        def origin(self):
            return r.get("origin") or ""

        def authorization_token(self):
            return r.get("token")

        def tools(self, slug, *, if_none_match=None):
            kind = r.get("listing") or "ok"
            if kind == "not_modified":
                return NotModified()
            if kind == "gateway_unavailable":
                raise GatewayUnavailable("down")
            if kind == "gateway_auth":
                raise GatewayAuthError("auth")
            if kind == "portal_unavailable":
                raise PortalToolsUnavailable("unavailable", code="X")
            if kind == "tool_gateway_error":
                raise ToolGatewayError("bad")
            return types.SimpleNamespace(
                connector=r.get("connector") or "connector",
                toolkit_version=r.get("toolkit_version") or "v1",
                etag=r.get("etag") or "etag",
                tools=[None] * int(r.get("tools") or 0))

    g = {
        "ToolsRead": ToolsRead,
        "NotModified": NotModified,
        "PortalToolsUnavailable": PortalToolsUnavailable,
        "GatewayUnavailable": GatewayUnavailable,
        "GatewayAuthError": GatewayAuthError,
        "ToolGatewayError": ToolGatewayError,
        "replace": dataclasses.replace,
        "TTL_SECONDS": 24 * 60 * 60,
        "time": time,
        "validate_slug": validate_slug_live,
        "_member_key": member_key_live,
        "_cache_path": lambda origin, key, slug: FakePath(),
        "_cached": lambda path: cached,
        "_store": store,
    }
    exec(compile(ast.Module(body=[READ_TOOLS_NODE], type_ignores=[]), str(SRC), "exec"), g)
    fn = g["read_tools"]
    try:
        res = fn(r.get("slug"), client=Client(), refresh=bool(r.get("refresh")),
                 now=lambda: float(r.get("now") or 0.0))
    except Exception as exc:  # noqa: BLE001 - the decision under test
        return {"source": "raise", "error": type(exc).__name__, "stale": False,
                "fetched_at": None, "stored": state["stored"],
                "unlinked": state["unlinked"]}
    return {"source": res.source, "stale": bool(res.stale), "fetched_at": res.fetched_at,
            "stored": state["stored"], "unlinked": state["unlinked"], "error": None}


def expected(r):
    f = r.get("func")
    if f == "member_key":
        return member_key_live(r.get("token"))
    if f == "slug_valid":
        try:
            validate_slug_live(r.get("slug"))
        except InvalidConnectorSlug:
            return 0
        return 1
    if f == "cache_rel":
        return cache_rel(r.get("origin") or "", r.get("member_key") or "", r.get("slug") or "")
    if f == "home_env":
        return (os.environ.get("SLERMES_HOME") or os.environ.get("HERMES_HOME")
                or os.environ.get("HOME") or None)
    if f == "read_plan":
        return read_plan(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
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
