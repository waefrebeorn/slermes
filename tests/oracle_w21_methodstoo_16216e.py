#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_tools_plugin_servers.py
(gate w21_methodstoo_16216e).

The three ported functions are lifted out of the LIVE source with the AST
extractor (sync/async defs and module assignments), and the collaborators the
module fetches lazily out of server.py globals -- `_tools_mod(...)`: the plugin
loader, the MCP client's `_core` ledgers, the toolsets table, config.yaml and
the secret scope behind `_interpolate_env_vars` -- are replaced by stubs reading
one injected state object per case. Those are exactly the config/network seams
the port also takes as input; the set algebra, the ${VAR} identity comparison
and the str() coercions are the live code and stay under test.

Stubbed seams:
  * tools.mcp_tool_config._interpolate_env_vars  -> ${VAR} subst driven by state["env"]
  * tools.mcp_tool_scope._resolve_server_key     -> identity (single-scope ledger)
  * tools.mcp_tool_common._core                  -> registered/lazy tool ledgers
  * hermes_cli.plugins (discover_plugins/get_plugin_manager), hermes_cli.config.load_config
  * toolsets._HERMES_CORE_TOOLS / TOOLSETS
"""
import __future__
import ast
import importlib.util
import json
import re
import sys
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tui_gateway" / "methods_tools_plugin_servers.py"
GATE = "w21_methodstoo_16216e"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

FUNCS = ("_server_identity", "_portable_server_tools", "_plugin_tool_ownership")

ENV_REF = re.compile(r"\$\{([^}]+)\}")


def ref_name(raw):
    """tools.mcp_tool_common._env_ref_name: strip, then a Cursor-style env: prefix."""
    ref = raw.strip()
    if ref.startswith("env:"):
        ref = ref[len("env:"):].strip()
    return ref


def make_interp(env):
    """tools.mcp_tool_config._interpolate_env_vars, minus the secret resolver."""

    def interp(value):
        if isinstance(value, str):
            def repl(m):
                return env.get(ref_name(m.group(1))) or m.group(0)
            return ENV_REF.sub(repl, value) if ENV_REF.search(value) else value
        if isinstance(value, dict):
            return {k: interp(v) for k, v in value.items()}
        if isinstance(value, list):
            return [interp(v) for v in value]
        return value

    return interp


def build_state(state):
    """Fake `_tools_mod(...)` namespaces over one injected state dict."""
    state = state if isinstance(state, dict) else {}
    env = state.get("env") or {}
    interp = make_interp(env if isinstance(env, dict) else {})
    registered = state.get("registered") or {}
    lazy = state.get("lazy") or {}
    servers = {}
    if isinstance(registered, dict):
        for name, tools in registered.items():
            servers[name] = SimpleNamespace(_registered_tool_names=list(tools or []))
    if isinstance(lazy, dict):
        for name, tools in lazy.items():
            if name not in servers:
                servers[name] = None

    core = SimpleNamespace(_servers=servers, _lazy_server_tool_names=dict(lazy or {}))
    portable_servers = state.get("portable_servers") or {}
    declared = state.get("declared") or {}
    native = state.get("native")

    manager = SimpleNamespace(
        get_portable_mcp_servers=lambda: dict(declared),
        get_portable_mcp_server_plugins=lambda: dict(portable_servers),
        _plugins={k: SimpleNamespace(tools_registered=list(v or []))
                  for k, v in (state.get("plugins") or {}).items()},
    )

    modules = {
        "tools.mcp_tool_config": SimpleNamespace(_interpolate_env_vars=interp),
        "tools.mcp_tool_scope": SimpleNamespace(_resolve_server_key=lambda name: name),
        "tools.mcp_tool_common": SimpleNamespace(_core=core),
        "hermes_cli.plugins": SimpleNamespace(discover_plugins=lambda: None,
                                             get_plugin_manager=lambda: manager),
        "hermes_cli.config": SimpleNamespace(
            load_config=lambda: {"mcp_servers": native}),
        "toolsets": SimpleNamespace(
            _HERMES_CORE_TOOLS=list(state.get("core_tools") or []),
            TOOLSETS={k: {"tools": list((v or {}).get("tools") or [])}
                      for k, v in (state.get("toolsets") or {}).items()}),
    }
    return SimpleNamespace(modules=modules, manager=manager, state=state)


NS = {}
CURRENT = {"state": {}, "env": {}}


def load_ns():
    NS.clear()
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in FUNCS:
            node = ast.parse(ast.unparse(node)).body[0]
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), NS)

    def tools_mod(name):
        built = build_state(CURRENT["state"])
        return built.modules[name]

    NS["_tools_mod"] = tools_mod


def canon_identity(v):
    """_server_identity returns (str, tuple, str) -> the JSON trio."""
    if isinstance(v, tuple) and len(v) == 3 and isinstance(v[1], tuple):
        return [v[0], list(v[1]), v[2]]
    return v


def canon_dict_of_sets(v):
    """{owner: set} -> {str(owner): sorted list} (JSON object keys are strings,
    so a non-string owner is compared in its wire form)."""
    if isinstance(v, dict):
        return {str(k): sorted(x) for k, x in v.items()}
    return v


def expected(r):
    f = r.get("func")
    if f == "identity":
        CURRENT["state"] = {"env": r.get("env") or {}}
        return canon_identity(NS["_server_identity"](r.get("cfg")))
    CURRENT["state"] = r.get("state") or {}
    built = build_state(CURRENT["state"])
    if f == "portable":
        native = built.state.get("native")
        return canon_dict_of_sets(
            NS["_portable_server_tools"](built.manager, native if isinstance(native, dict) else {}))
    if f == "ownership":
        return canon_dict_of_sets(NS["_plugin_tool_ownership"]())
    raise AssertionError("unknown func %r" % (f,))


def main():
    load_ns()
    cases = mism = 0
    wanted = {"identity", "portable", "ownership"}
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
