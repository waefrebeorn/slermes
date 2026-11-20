#!/usr/bin/env python3
"""Oracle for the tools_config.py UNCLAIMED FLOW port
(src/cli/tools_config_flow2.c, harness t_port_tools_config_flow2.c).

Drives the LIVE tools_config.py functions with SCRIPTED I/O: every collaborator
is a recording stub fed the same scripts the C harness feeds its seams. Compares
the DECISION SEQUENCE (ordered seam names in "trace"), the return ("ret"), the
mutated state mirror (restricted to seeded keys) and captured print output.
Live defs are AST-extracted (module Assign/AnnAssign + FunctionDef exec); the
heavy quarry import chain is never imported. Fail-closed: a missing seam script
raises RuntimeError("unscripted seam: <name>"); 0 cases => exit 1.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "tools_config.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "tools_config.py")

PY = {
    "_coerce_platform_toolsets_value": "_coerce_platform_toolsets_value",
    "_platform_toolsets_explicitly_saved": "_platform_toolsets_explicitly_saved",
    "_default_off_toolsets": "_default_off_toolsets",
    "_configurable_subset_of": "_configurable_subset_of",
    "_composite_toolsets": "_composite_toolsets",
    "_enabled_plugin_toolsets": "_enabled_plugin_toolsets",
    "_context_engine_active": "_context_engine_active",
    "_merge_mcp_servers": "_merge_mcp_servers",
    "_prune_toolsets_stripped_by_disabled": "_prune_toolsets_stripped_by_disabled",
    "_disabled_entry_tools": "_disabled_entry_tools",
    "_platform_default_toolset": "_platform_default_toolset",
    "_is_configurable": "_is_configurable",
    "_toolset_label": "_toolset_label",
    "_configurable_keys": "_configurable_keys",
    "_platform_default_keys": "_platform_default_keys",
    "_provider_env_ready": "_provider_env_ready",
    "_projected_plugin_toolsets": "_projected_plugin_toolsets",
    "_print_toolset_diff": "_print_toolset_diff",
    "_checklist_diff": "_checklist_diff",
    "_current_platform_tools": "_current_platform_tools",
    "_toolsets_needing_setup": "_toolsets_needing_setup",
    "_configure_newly_added": "_configure_newly_added",
}


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    cls = getattr(builtins, t, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        e = cls(msg)
    else:
        e = Exception(msg)
    if t == "ModuleNotFoundError":
        e.name = spec.get("name")
    return e


def exc_spec(e):
    spec = {"type": type(e).__name__, "message": str(e)}
    if isinstance(e, ModuleNotFoundError):
        spec["name"] = getattr(e, "name", None)
    return spec


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.stdout = []
        self.stderr = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})

    def peek(self, name):
        entry = self.io.get(name)
        if entry is None:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            return entry[idx] if idx < len(entry) else None
        return entry

    def consume(self, name):
        entry = self.io.get(name)
        if isinstance(entry, list):
            self.cursors[name] = self.cursors.get(name, 0) + 1

    def invoke(self, name):
        self.trace.append(name)
        entry = self.peek(name)
        if entry is None:
            raise RuntimeError("unscripted seam: " + name)
        self.consume(name)
        if isinstance(entry, dict):
            eff = entry.get("state")
            if isinstance(eff, dict):
                self.state.update(copy.deepcopy(eff))
            r = entry.get("raise")
            if isinstance(r, dict):
                raise make_exc(r)
            return entry.get("ret")
        return entry

    def recorder(self, name):
        ctx = self

        def call(*a, **k):
            return ctx.invoke(name)
        return call


CURRENT = {"ctx": None}
NS = {}
REAL = {}


def _ensure_pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def proj(v):
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(proj(x) for x in v)
    if isinstance(v, BaseException):
        return exc_spec(v)
    return v


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {
        "__name__": "hermes_cli.tools_config",
        "os": os, "sys": sys, "json": json, "copy": copy,
        "Any": object, "Dict": dict, "List": list, "Optional": object,
        "Set": set, "Tuple": tuple, "NoReturn": object,
        "Colors": type("Colors", (), {k: "" for k in
            ("GREEN", "RED", "DIM", "BOLD", "CYAN", "YELLOW", "RESET")}),
    }
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if node.name not in PY.values():
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    missing = [v for v in PY.values() if v not in ns]
    if missing:
        print("FATAL tools_config_flow2: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    REAL["_is_configurable"] = ns["_is_configurable"]
    return ns


def install_stubs(ctx, inp):
    ns = NS
    rec = ctx.recorder

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    _ensure_pkg("hermes_cli")
    _ensure_pkg("agent")

    # AUTO cosmetic seams.
    def print_stub(*a, file=None, **k):
        ctx.trace.append("print")
        text = a[0] if a else ""
        (ctx.stderr if file is sys.stderr else ctx.stdout).append(text)
        return None

    def color_stub(*a, **k):
        ctx.trace.append("color")
        return a[0] if a else ""

    ns["print"] = print_stub
    ns["color"] = color_stub

    # Simple seam collaborators (args are not part of the compared contract).
    for name in (
        "toolset_allowed_for_platform", "xai_credentials_present",
        "get_env_value", "validate_toolset",
        "get_effective_configurable_toolsets", "get_platform_tools",
        "toolset_needs_configuration_prompt", "configure_toolset",
    ):
        ns[name] = rec(name)
    ns["parse_platform_toolsets_value"] = rec("parse_toolsets_value")
    # Set-returning seams are materialized into Python sets (live does set ops).
    def set_seam(name):
        def call(*a, **k):
            v = ctx.invoke(name)
            return set(v) if isinstance(v, list) else set()
        return call
    ns["enabled_mcp_server_names"] = set_seam("enabled_mcp_server_names")
    ns["_checklist_toolset_keys"] = set_seam("checklist_toolset_keys")
    ns["_get_platform_tools"] = set_seam("get_platform_tools")
    # Composed pure helper is seamed under its own name; direct cases drive REAL.
    ns["_is_configurable"] = rec("is_configurable")
    ns["_toolset_allowed_for_platform"] = rec("toolset_allowed_for_platform")
    ns["_xai_credentials_present"] = rec("xai_credentials_present")
    ns["_get_effective_configurable_toolsets"] = rec("get_effective_configurable_toolsets")
    ns["_toolset_needs_configuration_prompt"] = rec("toolset_needs_configuration_prompt")
    ns["_configure_toolset"] = rec("configure_toolset")
    ns["logger"] = types.SimpleNamespace(warning=rec("logger.warning"), debug=rec("logger.debug"))

    # Mutating seam: model_tools._apply_toolset_selection mutates its set argument.
    def apply_selection(tools, disabled, *, quiet_mode=False, disable=False):
        kept = ctx.invoke("apply_toolset_selection")
        if not isinstance(kept, list):
            kept = []
        if disable:
            tools.clear()
            tools.update(kept)
        else:
            tools.update(kept)
        return None

    sys.modules["model_tools"] = mod("model_tools", _apply_toolset_selection=apply_selection)
    c = inp.get("constants") or {}
    sys.modules["toolsets"] = mod("toolsets",
        resolve_toolset=rec("resolve_toolset"),
        validate_toolset=rec("validate_toolset"),
        TOOLSETS={t[0]: {} for t in c.get("configurable_toolsets", [])})


def seed_state(ctx, inp):
    ns = NS
    c = inp.get("constants") or {}
    ns["CONFIGURABLE_TOOLSETS"] = copy.deepcopy(c.get("configurable_toolsets", []))
    ns["_DEFAULT_OFF_TOOLSETS"] = copy.deepcopy(c.get("default_off_toolsets", []))
    ns["_TOOLSET_PLATFORM_RESTRICTIONS"] = copy.deepcopy(c.get("toolset_platform_restrictions", {}))
    ns["PLATFORMS"] = copy.deepcopy(c.get("platforms", {}))
    ns["TOOL_CATEGORIES"] = copy.deepcopy(c.get("tool_categories", {}))
    ns["TOOLSET_ENV_REQUIREMENTS"] = copy.deepcopy(c.get("toolset_env_requirements", {}))
    ns["_warned_invalid_platform_toolsets"] = set(ctx.state.get("warned", []) or [])
    provider = bool(ctx.state.get("gateway_provider", False))
    if provider:
        def gateway_provider():
            return ctx.invoke("gateway_provider")
        ns["_gateway_plugin_toolsets"] = gateway_provider
    else:
        ns["_gateway_plugin_toolsets"] = None


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    install_stubs(ctx, inp)
    seed_state(ctx, inp)
    ns = NS
    out = {"ret": None, "trace": ctx.trace}
    g = inp.get

    try:
        if func == "_coerce_platform_toolsets_value":
            out["ret"] = ns["_coerce_platform_toolsets_value"](g("value"), g("platform", ""))
        elif func == "_platform_toolsets_explicitly_saved":
            out["ret"] = ns["_platform_toolsets_explicitly_saved"](g("config") or {}, g("platform", ""))
        elif func == "_default_off_toolsets":
            out["ret"] = ns["_default_off_toolsets"](g("platform", ""), bool(g("explicitly_configured", False)))
        elif func == "_configurable_subset_of":
            out["ret"] = ns["_configurable_subset_of"](set(g("tool_names", []) or []), g("platform", ""))
        elif func == "_composite_toolsets":
            out["ret"] = ns["_composite_toolsets"](g("toolset_names", []) or [], g("platform", ""),
                                                    bool(g("explicitly_configured", False)))
        elif func == "_enabled_plugin_toolsets":
            out["ret"] = ns["_enabled_plugin_toolsets"](g("config") or {}, g("platform", ""),
                g("toolset_names", []) or [], set(g("plugin_ts_keys", []) or []))
        elif func == "_context_engine_active":
            out["ret"] = ns["_context_engine_active"](g("config") or {})
        elif func == "_merge_mcp_servers":
            out["ret"] = ns["_merge_mcp_servers"](g("config") or {}, g("toolset_names", []) or [],
                set(g("explicit_passthrough", []) or []), bool(g("include_default_mcp_servers", False)))
        elif func == "_prune_toolsets_stripped_by_disabled":
            out["ret"] = ns["_prune_toolsets_stripped_by_disabled"](
                set(g("enabled_toolsets", []) or []), g("disabled_names", []) or [])
        elif func == "_disabled_entry_tools":
            out["ret"] = ns["_disabled_entry_tools"](g("name", ""))
        elif func == "_platform_default_toolset":
            out["ret"] = ns["_platform_default_toolset"](g("platform", ""))
        elif func == "_is_configurable":
            out["ret"] = REAL["_is_configurable"](g("ts_key", ""))
        elif func == "_toolset_label":
            out["ret"] = ns["_toolset_label"](g("ts_key", ""))
        elif func == "_configurable_keys":
            out["ret"] = ns["_configurable_keys"]()
        elif func == "_platform_default_keys":
            out["ret"] = ns["_platform_default_keys"]()
        elif func == "_provider_env_ready":
            out["ret"] = ns["_provider_env_ready"](g("provider") or {})
        elif func == "_projected_plugin_toolsets":
            out["ret"] = ns["_projected_plugin_toolsets"]()
        elif func == "_print_toolset_diff":
            out["ret"] = ns["_print_toolset_diff"](set(g("added", []) or []), set(g("removed", []) or []),
                                                    indent=g("indent", "  "))
        elif func == "_checklist_diff":
            out["ret"] = ns["_checklist_diff"](set(g("new_enabled", []) or []), set(g("prev", []) or []),
                                                g("platform", ""))
        elif func == "_current_platform_tools":
            out["ret"] = ns["_current_platform_tools"](g("config") or {}, g("pkey", ""))
        elif func == "_toolsets_needing_setup":
            out["ret"] = ns["_toolsets_needing_setup"](set(g("new_enabled", []) or []), g("config") or {})
        elif func == "_configure_newly_added":
            out["ret"] = ns["_configure_newly_added"](set(g("added", []) or []), set(g("already", []) or []),
                                                       g("config") or {})
        else:
            raise KeyError(func)
    except BaseException as e:
        out["ret"] = {"__raise__": exc_spec(e)}

    # Write the live warned-set back into the state window.
    if "warned" in ctx.state:
        ctx.state["warned"] = sorted(ns["_warned_invalid_platform_toolsets"])
    out["ret"] = proj(out["ret"])
    out["state"] = {k: proj(ctx.state[k]) for k in (inp.get("state") or {}) if k in ctx.state}
    out["stdout"] = ctx.stdout
    out["stderr"] = ctx.stderr
    return out


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out") or {}
        ok = (got.get("trace") == want.get("trace")
              and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state")
              and got.get("stdout") == want.get("stdout")
              and got.get("stderr") == want.get("stderr"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE tools_config_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    NS = extract()
    sys.exit(main())
