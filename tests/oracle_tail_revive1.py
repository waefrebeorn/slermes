#!/usr/bin/env python3
"""Differential oracle for the tail_revive1 revival wave (22 ports).

Reads the harness's JSONL stdout on stdin ({func, in, out, trace, st}),
recomputes the LIVE Python reference for each case from the echoed
"in" (AST-extracted defs + seeded Assign/AnnAssign constants, identical
scripted seams), and diffs out / ordered trace / post-state.

Fail-closed: extraction failures, constant drift vs the C pins, unknown
funcs and corrupt lines exit 2. BaseException (SystemExit!) is caught
per case and compared as {"__raise__": cls, "msg": str(exc)}.

Run:  python tests/oracle_tail_revive1.py < harness.out
Gate: see the header of tests/t_port_tail_revive1.c (explicit objs).
"""
import ast
import asyncio
import builtins
import copy
import json
import math
import os
import sys
import textwrap
import types
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

_REPO = Path(__file__).resolve().parent.parent
HERMES_ROOT = Path(r"C:/Users/eman5/hermes-agent")
if str(HERMES_ROOT) not in sys.path:
    sys.path.insert(0, str(HERMES_ROOT))
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

BASE_EXC = {
    "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError,
}


def fatal(msg):
    print(f"ORACLE tail_revive1 FATAL: {msg}", file=sys.stderr)
    sys.exit(2)


# ---------------------------------------------------------------- extraction

def extract(modpath, funcs=(), consts=(), classes=(), seed=None):
    """Exec selected live nodes into a registered module namespace."""
    path = HERMES_ROOT / modpath
    src = path.read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "tail1live_" + modpath.replace("/", "_").replace(".", "_")
    mod = types.ModuleType(modname)
    mod.__file__ = str(path)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seed or {})

    def exec_node(node):
        seg = ast.get_source_segment(src, node)
        if isinstance(node, ast.ClassDef) and node.decorator_list:
            # get_source_segment starts at `class` — decorators (e.g.
            # @dataclass) live above node.lineno and must be included.
            lines = src.splitlines()
            seg = "\n".join(lines[node.decorator_list[0].lineno - 1:node.end_lineno])
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.col_offset > 0:
            seg = textwrap.dedent(seg)
        exec(compile(seg, str(path), "exec"), ns)

    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in classes:
            exec_node(node)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in consts for t in targets):
                exec_node(node)
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in funcs:
            exec_node(node)
    # re-seed constants that reference extracted defs (partial aliases etc.)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in consts for t in targets):
                try:
                    exec_node(node)
                except Exception:
                    pass
    missing = [n for n in funcs if n not in ns] + [n for n in consts if n not in ns] \
            + [n for n in classes if n not in ns]
    if missing:
        fatal(f"{modpath}: failed to bind {missing}")
    return ns


def extract_consts_with_deps(modpath, consts):
    """Exec requested top-level nodes, resolving NameError gaps through
    sibling assignments/defs and top-level ImportFrom chains (the
    DEFAULT_CONFIG dict references constants and the imported _aux
    helper). Fail-closed when a name cannot be resolved."""
    parsed = {}
    namespaces = {}

    def module_ns(mp):
        if mp not in namespaces:
            path = HERMES_ROOT / mp
            src = path.read_text(encoding="utf-8")
            parsed[mp] = (src, ast.parse(src))
            modname = "tail1live_" + mp.replace("/", "_").replace(".", "_") + "_deps"
            mod = types.ModuleType(modname)
            mod.__file__ = str(path)
            sys.modules[modname] = mod
            namespaces[mp] = mod.__dict__
        return namespaces[mp]

    def find_node(mp, name):
        src, tree = parsed[mp]
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                targets = node.targets if isinstance(node, ast.Assign) else [node.target]
                if any(isinstance(t, ast.Name) and t.id == name for t in targets):
                    return ("node", node)
            elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)) \
                    and node.name == name:
                return ("node", node)
            elif isinstance(node, ast.ImportFrom):
                for alias in node.names:
                    if (alias.asname or alias.name) == name:
                        return ("import", node.module, alias.name)
        return None

    def load_name(mp, name, depth=0):
        ns = module_ns(mp)
        if name in ns:
            return ns[name]
        if depth > 24:
            fatal(f"{mp}: dependency chain too deep at {name!r}")
        found = find_node(mp, name)
        if found is None:
            fatal(f"{mp}: cannot seed {name!r}")
        if found[0] == "import":
            _, srcmod, orig = found
            srcpath = srcmod.replace(".", "/") + ".py"
            val = load_name(srcpath, orig, depth + 1)
            ns[name] = val
            return val
        node = found[1]
        src, _ = parsed[mp]
        seg = ast.get_source_segment(src, node)
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.col_offset > 0:
            seg = textwrap.dedent(seg)
        for _ in range(32):
            try:
                exec(compile(seg, str(HERMES_ROOT / mp), "exec"), ns)
                return ns[name]
            except NameError as e:
                load_name(mp, e.name, depth + 1)
        fatal(f"{mp}: {name!r} still unresolvable")

    ns = module_ns(modpath)
    for c in consts:
        load_name(modpath, c)
    return ns


def stub_package(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


# in-function import targets (seamed, never the real modules)
stub_package("agent")
MS = types.ModuleType("agent.message_sanitization")
sys.modules["agent.message_sanitization"] = MS
sys.modules["agent"].message_sanitization = MS
stub_package("gateway")
SC = types.ModuleType("gateway.session_context")
sys.modules["gateway.session_context"] = SC
sys.modules["gateway"].session_context = SC

try:
    NS_CC = extract("agent/context_compressor.py",
                    funcs=("_compression_cancelled", "_is_hygiene_preagent_only_cooldown",
                           "_stale_thinking_on_wire", "_billed_basis_over_threshold"),
                    consts=("_HYGIENE_PREAGENT_ONLY_COOLDOWN_MARKERS",))
    NS_TC = extract("agent/turn_context.py",
                    funcs=("_agent_stale_thinking_on_wire",
                           "_review_fork_first_request_pending", "_str_attr"),
                    seed={"Any": Any})
    NS_EST = extract("hermes_cli/local_runtime/estimator.py",
                     funcs=("ctx_bytes", "per_token_kv_f16", "recurrent_layer_count",
                            "kv_dtype_factor"),
                     consts=("_Q8_BYTES_PER_ELEM", "_F16_BYTES_PER_ELEM",
                             "_RECURRENT_STATE_PER_LAYER"),
                     classes=("LayerKind", "ModelProfile"),
                     seed={"Enum": Enum, "dataclass": dataclass, "field": field})
    NS_HW = extract("hermes_cli/local_runtime/hardware.py",
                    funcs=("_unified_pool_bytes", "_nvidia_vram"),
                    consts=("_POOL_DISAGREEMENT_FACTOR", "_POOL_RAM_FRACTION"))
    NS_WRC = extract("tools/web_result_cache.py",
                     funcs=("_is_cache_exempt_host", "_host_matches_pattern"),
                     seed={"urlparse": urlparse})
    # DEFAULT_CONFIG is imported by gateway/restart.py from config_defaults;
    # exec just that Assign (fail-closed), then the two restart constants.
    NS_CFGD = extract_consts_with_deps("hermes_cli/config_defaults.py", ("DEFAULT_CONFIG",))
    NS_RG = extract("gateway/restart.py",
                    funcs=("resolve_cron_drain_budget", "is_global_startup_conflict",
                           "parse_signal_interrupt_grace_timeout", "_seconds",
                           "_parse_timeout_keeping_zero"),
                    consts=("CRON_DRAIN_CLEANUP_RESERVE_S",
                            "DEFAULT_GATEWAY_SIGNAL_INTERRUPT_GRACE_TIMEOUT"),
                    seed={"math": math, "DEFAULT_CONFIG": NS_CFGD["DEFAULT_CONFIG"]})
    NS_LEP = extract("tools/environments/local_env_policy.py",
                     funcs=("_buzz_terminal_context_active", "_is_terminal_first_party_env",
                            "_matches_terminal_first_party_prefix"),
                     consts=("_TERMINAL_FIRST_PARTY_ENV_PREFIXES",),
                     seed={"os": os})
    NS_PB = extract("agent/prompt_builder.py",
                    funcs=("_skill_should_show", "_clear_backend_probe_cache"),
                    consts=("_BACKEND_PROBE_CACHE",))
except SystemExit:
    raise
except BaseException as e:
    fatal(f"extraction failed: {type(e).__name__}: {e}")

# C-pin drift checks (fail closed, never hardcode silently)
if NS_RG["CRON_DRAIN_CLEANUP_RESERVE_S"] != 10.0:
    fatal("CRON_DRAIN_CLEANUP_RESERVE_S drifted from C pin 10.0")
if NS_RG["DEFAULT_GATEWAY_SIGNAL_INTERRUPT_GRACE_TIMEOUT"] != 1.0:
    fatal("DEFAULT_GATEWAY_SIGNAL_INTERRUPT_GRACE_TIMEOUT drifted from C pin 1.0")
if NS_HW["_POOL_DISAGREEMENT_FACTOR"] != 1.5 or NS_HW["_POOL_RAM_FRACTION"] != 0.75:
    fatal("hardware pool constants drifted from C pins")
if NS_EST["_RECURRENT_STATE_PER_LAYER"] != (4 << 20):
    fatal("_RECURRENT_STATE_PER_LAYER drifted from C pin")
if abs(NS_EST["_Q8_BYTES_PER_ELEM"] / NS_EST["_F16_BYTES_PER_ELEM"] - 0.53125) > 1e-12:
    fatal("kv dtype factor drifted from C pin 0.53125")
if tuple(NS_CC["_HYGIENE_PREAGENT_ONLY_COOLDOWN_MARKERS"]) != (
        "session hygiene compression timed out",
        "hygiene compression deferred: turn-hold budget expired"):
    fatal("hygiene cooldown markers drifted from C pins")
if tuple(NS_LEP["_TERMINAL_FIRST_PARTY_ENV_PREFIXES"]) != ("BUZZ_",):
    fatal("_TERMINAL_FIRST_PARTY_ENV_PREFIXES drifted from C pin")


# ---------------------------------------------------------------- seam scripts

def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    cls = BASE_EXC.get(kind)
    if cls is None:
        cand = getattr(builtins, kind, None)
        if isinstance(cand, type) and issubclass(cand, BaseException):
            cls = cand
        else:
            cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        return cls(msg)  # str(KeyError(k)) == repr(k)
    return cls(msg)


class CaseCtx:
    def __init__(self, io):
        self.io = io or {}
        self.trace = []
        self.counts = {}

    def script_value(self, name):
        if name not in self.io:
            raise RuntimeError(f"unscripted seam: {name}")
        script = self.io[name]
        if isinstance(script, list):
            idx = self.counts.get(name, 0)
            if idx >= len(script):
                raise RuntimeError(f"script exhausted: {name}")
            self.counts[name] = idx + 1
            return script[idx]
        if isinstance(script, dict) and script.get("__script__"):
            return script.get("ret")
        return script

    def recorder(self, name):
        def call(*args):
            try:
                raw = self.script_value(name)
            except RuntimeError as e:
                self.trace.append({"ev": name, "args": list(args),
                                   "ret": {"__raise__": "RuntimeError", "msg": str(e)}})
                raise
            self.trace.append({"ev": name, "args": list(args), "ret": copy.deepcopy(raw)})
            if isinstance(raw, dict) and "__raise__" in raw:
                raise make_exc(raw)
            return copy.deepcopy(raw)
        return call


class LoggerDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def debug(self, msg, *a, **k):
        self.ctx.trace.append({"ev": "log_debug", "args": [msg], "ret": None})

    def warning(self, msg, *a, **k):
        self.ctx.trace.append({"ev": "log_warning", "args": [msg], "ret": None})


class ContextCompressor:
    """Named double so AttributeError messages match the C port."""
    pass


def norm(x):
    """Integral floats -> int (JSON carries no int/float identity);
    tuples -> lists; sets -> sorted lists."""
    if isinstance(x, bool):
        return x
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x):
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, set):
        return sorted(norm(v) for v in x)
    return x


# ---------------------------------------------------------------- live drive

def drive(rec):
    func = rec["func"]
    inp = rec.get("in") or {}
    ctx = CaseCtx(inp.get("io"))
    state = inp.get("state") or {}
    old_environ = os.environ
    st = {}
    try:
        # per-case seam installs (into the SAME namespaces the defs live in)
        MS.stale_thinking_reaches_wire = ctx.recorder("stale_thinking_reaches_wire")
        SC.get_session_env = ctx.recorder("get_session_env")
        NS_CC["logger"] = LoggerDouble(ctx)
        NS_TC["logger"] = LoggerDouble(ctx)
        NS_HW["_device_pool_view"] = ctx.recorder("device_pool_view")
        NS_HW["_cached_nvidia_gpu_query"] = ctx.recorder("cached_nvidia_gpu_query")
        NS_WRC["_web_config"] = ctx.recorder("web_config")

        if func == "compression_cancelled":
            self = ContextCompressor()
            if "_compression_cancelled_check" in state:
                v = state["_compression_cancelled_check"]
                self._compression_cancelled_check = (
                    ctx.recorder("cancelled_check") if v == "fn" else v)
            out = NS_CC["_compression_cancelled"](self)
        elif func == "is_hygiene_preagent_only_cooldown":
            out = NS_CC["_is_hygiene_preagent_only_cooldown"](inp.get("error"))
        elif func == "stale_thinking_on_wire":
            out = NS_CC["_stale_thinking_on_wire"](types.SimpleNamespace(**state))
        elif func == "billed_basis_over_threshold":
            self = ContextCompressor()
            if "threshold_tokens" in state:
                self.threshold_tokens = state["threshold_tokens"]
            out = NS_CC["_billed_basis_over_threshold"](self, inp.get("current_tokens"))
        elif func == "agent_stale_thinking_on_wire":
            out = NS_TC["_agent_stale_thinking_on_wire"](types.SimpleNamespace(**state))
        elif func == "review_fork_first_request_pending":
            out = NS_TC["_review_fork_first_request_pending"](types.SimpleNamespace(**state))
        elif func == "ctx_bytes":
            p = inp.get("profile") or {}
            prof = NS_EST["ModelProfile"](
                name="t", weights_bytes=0, embd_table_bytes=0, n_ctx_train=0,
                layers=[(NS_EST["LayerKind"](k), b) for k, b in p.get("layers", [])],
                swa_window=p.get("swa_window", 0), kv_scale=p.get("kv_scale", 1.0),
                window_compute_per_token=p.get("window_compute_per_token", 0),
                kv_q8_ok=p.get("kv_q8_ok", True))
            out = NS_EST["ctx_bytes"](prof, inp["window"],
                                      flash_attention=inp.get("flash_attention", True))
        elif func == "per_token_kv_f16":
            layers = [(NS_EST["LayerKind"](k), b) for k, b in inp.get("layers", [])]
            out = NS_EST["per_token_kv_f16"](types.SimpleNamespace(layers=layers))
        elif func == "recurrent_layer_count":
            layers = [(NS_EST["LayerKind"](k), b) for k, b in inp.get("layers", [])]
            out = NS_EST["recurrent_layer_count"](types.SimpleNamespace(layers=layers))
        elif func == "kv_dtype_factor":
            out = NS_EST["kv_dtype_factor"](inp["flash_attention"], inp.get("kv_q8_ok", True))
        elif func == "unified_pool_bytes":
            out = NS_HW["_unified_pool_bytes"](inp["smi_total"], inp["ram_total"])
        elif func == "nvidia_vram":
            out = NS_HW["_nvidia_vram"]()
        elif func == "is_cache_exempt_host":
            out = NS_WRC["_is_cache_exempt_host"](inp["url"])
        elif func == "host_matches_pattern":
            out = NS_WRC["_host_matches_pattern"](inp["host"], inp.get("pattern"))
        elif func == "resolve_cron_drain_budget":
            out = NS_RG["resolve_cron_drain_budget"](
                inp["drain_timeout"], inp["cron_drain_timeout"],
                watchdog_delay=inp["watchdog_delay"],
                elapsed=inp.get("elapsed"),
                cleanup_reserve_s=inp.get("cleanup_reserve_s"))
        elif func == "is_global_startup_conflict":
            out = NS_RG["is_global_startup_conflict"](inp.get("error_code"))
        elif func == "parse_signal_interrupt_grace_timeout":
            out = NS_RG["parse_signal_interrupt_grace_timeout"](inp.get("raw"))
        elif func == "buzz_terminal_context_active":
            os.environ = dict(inp.get("env") or {})
            out = NS_LEP["_buzz_terminal_context_active"]()
        elif func == "is_terminal_first_party_env":
            os.environ = dict(inp.get("env") or {})
            out = NS_LEP["_is_terminal_first_party_env"](inp["name"])
        elif func == "matches_terminal_first_party_prefix":
            out = NS_LEP["_matches_terminal_first_party_prefix"](inp["name"])
        elif func == "skill_should_show":
            tools = inp.get("available_tools")
            toolsets = inp.get("available_toolsets")
            out = NS_PB["_skill_should_show"](
                inp.get("conditions") or {},
                None if tools is None else set(tools),
                None if toolsets is None else set(toolsets),
                inp.get("session_platform"))
        elif func == "clear_backend_probe_cache":
            NS_PB["_BACKEND_PROBE_CACHE"] = dict(state.get("cache") or {})
            NS_PB["_clear_backend_probe_cache"]()
            out = None
            st = NS_PB["_BACKEND_PROBE_CACHE"]
        else:
            fatal(f"unknown func {func}")
    except BaseException as e:  # SystemExit included — fail-closed per case
        out = {"__raise__": type(e).__name__, "msg": str(e)}
    finally:
        os.environ = old_environ
    return {"func": func, "in": inp, "out": norm(out),
            "trace": norm(ctx.trace), "st": norm(st)}


# ---------------------------------------------------------------- comparison

def main():
    n = 0
    mismatches = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        n += 1
        try:
            got = json.loads(line)
        except ValueError as e:
            print(f"CASE {n}: harness line is not valid JSON: {e}")
            mismatches += 1
            continue
        func = got.get("func", "?")
        try:
            want = drive(got)
        except SystemExit:
            raise
        except BaseException as e:
            print(f"CASE {n} ({func}): oracle drive crashed: {type(e).__name__}: {e}")
            mismatches += 1
            continue
        diffs = []
        if got.get("func") != want["func"]:
            diffs.append(("func", got.get("func"), want["func"]))
        if norm(got.get("in")) != want["in"]:
            diffs.append(("in-echo", norm(got.get("in")), want["in"]))
        if norm(got.get("out")) != want["out"]:
            diffs.append(("out", norm(got.get("out")), want["out"]))
        if norm(got.get("trace")) != want["trace"]:
            diffs.append(("trace", norm(got.get("trace")), want["trace"]))
        if norm(got.get("st")) != want["st"]:
            diffs.append(("st", norm(got.get("st")), want["st"]))
        if diffs:
            mismatches += 1
            if mismatches <= 20:
                print(f"MISMATCH case {n} func={func}")
                for field_, g, w in diffs:
                    print(f"  {field_}:\n    C : {json.dumps(g, sort_keys=True)}"
                          f"\n    PY: {json.dumps(w, sort_keys=True)}")
    print(f"ORACLE tail_revive1: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
