#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_discovery.py (w11_mcptooldi_fd99f7 gate).

Loads the LIVE module via spec_from_file_location with a parent-package
bootstrap and stubs for all tools.mcp_tool_* imports. Origin _core state is
injected per case; _server_key/_resolve_server_key are identity (single scope).
Only pure bookkeeping/status helpers are exercised.
"""
import importlib.util
import json
import sys
import types
from contextlib import nullcontext
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

TRUE_WORDS = {"true", "1", "yes", "on"}
FALSE_WORDS = {"false", "0", "no", "off"}


def parse_boolish(value, default=True):
    if value is None:
        return default
    if isinstance(value, (bool, int, float)):
        return bool(value)
    if isinstance(value, str):
        low = value.strip().lower()
        if low in TRUE_WORDS:
            return True
        if low in FALSE_WORDS:
            return False
    return default


# parent-package bootstrap + sys.modules registration.
tools = types.ModuleType("tools"); tools.__path__ = []
sys.modules["tools"] = tools

CORE = SimpleNamespace(
    _servers={}, _server_connect_errors={}, _server_connecting=set(),
    _lazy_server_configs={}, _server_connect_retry_after={},
    _parallel_safe_servers=set(), _mcp_tool_server_names={},
    _lock=nullcontext(), _mcp_registry_scope=lambda: None,
)
common = types.ModuleType("tools.mcp_tool_common")
common._core = CORE
common._parse_boolish = parse_boolish
common.mcp_server_enabled = lambda cfg: (parse_boolish(cfg.get("enabled", True)) and
                                          not parse_boolish(cfg.get("disabled", False), default=False))
sys.modules["tools.mcp_tool_common"] = common
tools.mcp_tool_common = common

for name in ("mcp_tool_config", "mcp_tool_errors", "mcp_tool_lifecycle", "mcp_tool_loop", "mcp_tool_registration"):
    m = types.ModuleType("tools." + name)
    sys.modules["tools." + name] = m
    setattr(tools, name, m)

schema = types.ModuleType("tools.mcp_tool_schema")
schema.MCP_TOOL_NAME_PREFIX = "mcp__"
sys.modules["tools.mcp_tool_schema"] = schema
tools.mcp_tool_schema = schema

scope = types.ModuleType("tools.mcp_tool_scope")
scope._key_name = lambda k: k
scope._key_scope = lambda k: None
scope._key_visible_in_scope = lambda k, s: True
scope._resolve_server_key = lambda n, scope=None, current=True: n
scope._server_key = lambda n, scope=None, current=True: n
sys.modules["tools.mcp_tool_scope"] = scope
tools.mcp_tool_scope = scope

SRC = Q + "/tools/mcp_tool_discovery.py"
spec = importlib.util.spec_from_file_location("disc_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["disc_live"] = live
spec.loader.exec_module(live)


def install(r):
    CORE._servers = {
        n: SimpleNamespace(_registered_tool_names=["x"] * int(d.get("tools", 0)),
                           _park_reason=("p" if d.get("park") else None))
        for n, d in (r.get("servers") or {}).items()
    }
    CORE._server_connect_errors = dict(r.get("connect_errors") or {})
    CORE._server_connecting = set(r.get("connecting") or [])
    CORE._lazy_server_configs = {n: {} for n in (r.get("lazy_configs") or [])}
    CORE._server_connect_retry_after = dict(r.get("retry_after") or {})
    CORE._parallel_safe_servers = set(r.get("parallel_safe") or [])
    CORE._mcp_tool_server_names = dict(r.get("mcp_tool_server_names") or {})
    live.time = SimpleNamespace(monotonic=lambda: float(r.get("now", 0.0)))


def drive(r):
    func = r.get("func")
    install(r)
    if func == "connected_summary":
        tc, cc, failed = live._connected_summary(
            r.get("names") or [], lazy_tools=int(r.get("lazy_tools", 0)),
            lazy_servers=int(r.get("lazy_servers", 0)))
        return {"tool_count": tc, "connected": cc, "failed": [list(x) for x in failed]}
    if func == "unregistered":
        return sorted(live._unregistered(set(r.get("wanted") or []), None))
    if func == "awaiting_connect":
        return live._awaiting_connect(set(r.get("wanted") or []), None)
    if func == "not_connected":
        return sorted(live._not_connected(set(r.get("wanted") or []), None))
    if func == "parallel_safe":
        return live.is_mcp_tool_parallel_safe(r.get("tool_name", ""))
    if func == "resolve_server_lazy":
        return live._resolve_server_lazy("s", r.get("config") or {})
    return "UNKNOWN_FUNC"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    try:
        py = drive(r)
    except BaseException as exc:
        print(f"LIVE ERROR {r.get('func')}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w11_mcptooldi_fd99f7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
