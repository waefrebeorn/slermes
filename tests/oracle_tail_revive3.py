#!/usr/bin/env python3
"""oracle_tail_revive3.py — differential oracle for the tail group-3 revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the
LIVE quarry defs, seeds module constants (Assign + AnnAssign, plain-Name
targets only), drives each live function under the SAME scripted seam
results the C harness consumed, and compares ordered trace + ret.
BaseException is caught per case (SystemExit included); missing/exhausted
scripts fail closed on both sides.

Covered ports (16): is_lightpanda_process, desktop_loopback_auth_exempt,
is_openai_family_main, looks_like_opaque_credential,
has_top_level_alternation, has_nested_unbounded_repeat,
in_excluded_root_dir, is_oauth_pool_payload, query_is_concrete,
content_type_is_sse, is_managed_endpoint, uses_room_run_auth,
memory_provider_tools_exposed, uses_hermes_python_environment,
strict_path_exists, metadata (curator_merge_request_overrides).

Documented fixture scope (mirrors the port headers): ASCII strip/lower;
scalar str() projections (container values excluded where a full Python
repr would be needed); PurePosixPath.parts for backup paths; integral
doubles model Python ints (libjson cannot distinguish 1 from 1.0).

Gate (explicit objects — libjson only):
  bash tests/run_one_oracle.sh tail_revive3 oracle_tail_revive3.py "" "lib/libjson/json.o"
"""
import ast
import builtins
import __future__
import json
import math
import os
import re
import sys
import types
from pathlib import Path, PurePosixPath

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_tr3"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
    """Integral floats -> ints (libjson emits %.0f); tuples -> lists."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    return x


class Raise:
    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        self.spec = {"__raise__": ty, "message": msg}
        cls = getattr(builtins, ty, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(ty, (Exception,), {})
        self.exc = cls(msg)


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            raise RuntimeError(f"no script for {name}")
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = i + 1
            entry = entry[i]
        if isinstance(entry, dict) and entry.get("__script__"):
            if "raise" in entry:
                return Raise(entry["raise"])
            return entry.get("ret")
        return entry


def make_stub(trace, script, name, argmap):
    """Recording seam stub: identical event shape to the C dispatcher.
    argmap is a fixed args dict or a callable building it from the call."""
    def f(*a, **k):
        args = dict(argmap(*a, **k)) if callable(argmap) else dict(argmap)
        try:
            entry = script.next(name)
        except RuntimeError as e:
            spec = {"__raise__": "RuntimeError", "message": str(e)}
            trace.append({"ev": name, "args": args, "ret": spec})
            raise
        if isinstance(entry, Raise):
            trace.append({"ev": name, "args": args, "ret": entry.spec})
            raise entry.exc
        trace.append({"ev": name, "args": args, "ret": entry})
        return entry
    return f


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def base_seeds(modname):
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None))
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "Literal": object, "Mapping": object, "Protocol": object, "Tuple": object,
        "Awaitable": object, "Union": object,
        "dataclass": __import__("dataclasses").dataclass,
        "field": __import__("dataclasses").field,
        "contextlib": __import__("contextlib"),
        "logging": logging_double,
        "math": math, "re": re, "json": json,
        "os": types.SimpleNamespace(environ={}, path=types.SimpleNamespace(
            realpath=lambda p: p)),
        "sys": sys,
    }


def load_ns(relpath, seeds, want_funcs=(), want_consts=()):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_tr3_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    for n in tree.body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)) and n.value is not None:
            tgts = n.targets if isinstance(n, ast.Assign) else [n.target]
            if any(isinstance(t, ast.Name) and t.id in want_consts for t in tgts):
                exec(compile_node(n, relpath), ns)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            exec(compile_node(n, relpath), ns)
    for name in want_funcs:
        assert name in ns, f"seed failure: {relpath}:{name}"
    for name in want_consts:
        assert name in ns, f"const seed failure: {relpath}:{name}"
    return ns


def pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


# ── module namespaces (built once) ──────────────────────────────────────────

NS_LP = load_ns("tools/browser_lightpanda.py", base_seeds("quarry_tr3_lp"),
                want_funcs=("_is_lightpanda_process",))
NS_WSB = load_ns("hermes_cli/web_server_boundary.py", base_seeds("quarry_tr3_wsb"),
                 want_funcs=("_desktop_loopback_auth_exempt",),
                 want_consts=("_LOOPBACK_HOST_VALUES",))
NS_FT = load_ns("tools/file_tools.py", base_seeds("quarry_tr3_ft"),
                want_funcs=("_is_openai_family_main",))
NS_RED = load_ns("agent/redact.py", base_seeds("quarry_tr3_red"),
                 want_funcs=("_looks_like_opaque_credential",
                             "_has_top_level_alternation",
                             "_has_nested_unbounded_repeat"))
# _pattern_structure + helpers live in agent/redact_pattern_shape.py; bind
# them into the redact namespace (the wrappers call the module global).
for _n in ast.parse((QUARRY / "agent/redact_pattern_shape.py").read_text(
        encoding="utf-8")).body:
    if isinstance(_n, ast.FunctionDef) and _n.name in (
            "_pattern_structure", "_skip_char_class",
            "_unbounded_quantifier_follows"):
        exec(compile_node(_n, "agent/redact_pattern_shape.py"), NS_RED)
assert "_pattern_structure" in NS_RED

NS_HDL = load_ns("hermes_cli/home_data_layout.py", base_seeds("quarry_tr3_hdl"),
                 want_funcs=("profile_root_entry",),
                 want_consts=("BRIDGE_LOG_PARTS", "PM_RUNTIME_ROOT_DIRS",
                              "SECRET_CACHE_FILES"))
NS_HC = load_ns("hermes_constants.py", base_seeds("quarry_tr3_hc"),
                want_consts=("LOCAL_RUNTIME_ROOT_DIRS",))
NS_PM = load_ns("agent/provider_media.py", base_seeds("quarry_tr3_pm"),
                want_consts=("GENERATED_SUBDIR",))
_bak_seeds = base_seeds("quarry_tr3_bak")
_bak_seeds.update(
    profile_root_entry=NS_HDL["profile_root_entry"],
    BRIDGE_LOG_PARTS=NS_HDL["BRIDGE_LOG_PARTS"],
    PM_RUNTIME_ROOT_DIRS=NS_HDL["PM_RUNTIME_ROOT_DIRS"],
    SECRET_CACHE_FILES=NS_HDL["SECRET_CACHE_FILES"],
    LOCAL_RUNTIME_ROOT_DIRS=NS_HC["LOCAL_RUNTIME_ROOT_DIRS"],
    GENERATED_SUBDIR=NS_PM["GENERATED_SUBDIR"])
NS_BAK = load_ns("hermes_cli/backup.py", _bak_seeds,
                 want_funcs=("_in_excluded_root_dir",),
                 want_consts=("_EXCLUDED_ROOT_DIRS", "_EXCLUDED_BACKUP_ROOT_DIRS",
                              "_KEPT_CACHE_SUBDIRS"))
NS_AUTH = load_ns("hermes_cli/auth_oauth_grants.py", base_seeds("quarry_tr3_auth"),
                  want_funcs=("_is_oauth_pool_payload",))
NS_SH = load_ns("tools/skills_hub_models.py", base_seeds("quarry_tr3_sh"),
                want_funcs=("_query_is_concrete",),
                want_consts=("_VALUELESS_QUERY_FLAG_RE",))
NS_SSE = load_ns("hermes_cli/proxy/sse_done.py", base_seeds("quarry_tr3_sse"),
                 want_funcs=("content_type_is_sse",))
_gr_seeds = base_seeds("quarry_tr3_gr")
_gr_seeds["suppress"] = __import__("contextlib").suppress  # from contextlib import suppress
NS_GR = load_ns("hermes_cli/local_runtime/growth.py", _gr_seeds,
                want_funcs=("is_managed_endpoint",))
NS_AR = load_ns("gateway/platforms/api_server_runs.py", base_seeds("quarry_tr3_ar"),
                want_funcs=("_uses_room_run_auth",))
_mm_seeds = base_seeds("quarry_tr3_mm")
_mm_seeds["logger"] = _mm_seeds["logging"].getLogger("agent.memory_manager_tools")
NS_MM = load_ns("agent/memory_manager_tools.py", _mm_seeds,
                want_funcs=("memory_provider_tools_exposed", "_tool_name",
                            "memory_provider_tools_enabled"))
NS_CUR = load_ns("agent/curator.py", base_seeds("quarry_tr3_cur"),
                 want_funcs=("_merge_request_overrides",))
NS_CE = load_ns("tools/code_execution_env.py", base_seeds("quarry_tr3_ce"),
                want_funcs=("_uses_hermes_python_environment",))
NS_ST = load_ns("gateway/status.py", base_seeds("quarry_tr3_st"),
                want_funcs=("_strict_path_exists",))

# ── sys.modules doubles for in-function imports ─────────────────────────────

MOD_PSUTIL = pkg("psutil")
pkg("gateway")
MOD_GW_STATUS = pkg("gateway.status")
sys.modules["gateway"].status = MOD_GW_STATUS
pkg("agent")
MOD_AUX = pkg("agent.auxiliary_client")
sys.modules["agent"].auxiliary_client = MOD_AUX
pkg("hermes_cli")
pkg("hermes_cli.local_runtime")
MOD_ENDPOINT = pkg("hermes_cli.local_runtime.endpoint")
sys.modules["hermes_cli"].local_runtime = sys.modules["hermes_cli.local_runtime"]
sys.modules["hermes_cli.local_runtime"].endpoint = MOD_ENDPOINT
MOD_TOOLSETS = pkg("toolsets")


class ProcDouble:
    def __init__(self, trace, script, pid):
        self.name = make_stub(trace, script, "proc_name", {"pid": pid})
        self.cmdline = make_stub(trace, script, "proc_cmdline", {"pid": pid})


# ── per-case drive ──────────────────────────────────────────────────────────

def drive(func, inp, trace):
    script = Script(inp.get("io"))

    if func == "is_lightpanda_process":
        pid = inp.get("pid", 0)
        rec = make_stub(trace, script, "psutil_process",
                        lambda p: {"pid": p})

        def Process(p):
            rec(p)  # records + raises per script; handle itself is unused
            return ProcDouble(trace, script, p)

        MOD_PSUTIL.Process = Process
        MOD_GW_STATUS.get_process_start_time = make_stub(
            trace, script, "get_process_start_time", lambda p: {"pid": p})
        ret = NS_LP["_is_lightpanda_process"](
            pid, inp.get("port"), inp.get("start_time"))

    elif func == "desktop_loopback_auth_exempt":
        NS_WSB["os"] = types.SimpleNamespace(environ=dict(inp.get("env") or {}))
        ret = NS_WSB["_desktop_loopback_auth_exempt"](
            inp.get("host"),
            ssh_session_token=inp.get("ssh_session_token"),
            ssh_owner_nonce=inp.get("ssh_owner_nonce"))

    elif func == "is_openai_family_main":
        MOD_AUX._read_main_provider = make_stub(
            trace, script, "read_main_provider", {})
        MOD_AUX._read_main_model = make_stub(
            trace, script, "read_main_model", {})
        ret = NS_FT["_is_openai_family_main"]()

    elif func == "looks_like_opaque_credential":
        ret = NS_RED["_looks_like_opaque_credential"](inp.get("value"))

    elif func == "has_top_level_alternation":
        ret = NS_RED["_has_top_level_alternation"](inp.get("pattern"))

    elif func == "has_nested_unbounded_repeat":
        ret = NS_RED["_has_nested_unbounded_repeat"](inp.get("pattern"))

    elif func == "in_excluded_root_dir":
        ret = NS_BAK["_in_excluded_root_dir"](PurePosixPath(inp.get("path", "")))

    elif func == "is_oauth_pool_payload":
        ret = NS_AUTH["_is_oauth_pool_payload"](inp.get("entry"))

    elif func == "query_is_concrete":
        ret = NS_SH["_query_is_concrete"](inp.get("query"))

    elif func == "content_type_is_sse":
        ret = NS_SSE["content_type_is_sse"](inp.get("headers"))

    elif func == "is_managed_endpoint":
        MOD_ENDPOINT._state_endpoint = make_stub(
            trace, script, "state_endpoint", {})
        ret = NS_GR["is_managed_endpoint"](inp.get("base_url"))

    elif func == "uses_room_run_auth":
        path = inp.get("path", "")
        self_d = types.SimpleNamespace(
            _room_grant_token=make_stub(trace, script, "room_grant_token",
                                        {"path": path}))
        request_d = types.SimpleNamespace(path=path)
        ret = NS_AR["_uses_room_run_auth"](self_d, request_d)

    elif func == "memory_provider_tools_exposed":
        ag = inp.get("agent")
        if isinstance(ag, dict):
            agent_d = types.SimpleNamespace(**{
                k: ag[k] for k in ("tools", "enabled_toolsets",
                                   "disabled_toolsets") if k in ag})
        else:
            agent_d = ag
        MOD_TOOLSETS.resolve_toolset = make_stub(
            trace, script, "resolve_toolset", lambda name: {"name": name})
        ret = NS_MM["memory_provider_tools_exposed"](agent_d)

    elif func == "uses_hermes_python_environment":
        NS_CE["sys"] = types.SimpleNamespace(
            executable=inp.get("sys_executable"), prefix=inp.get("sys_prefix"))
        NS_CE["os"] = types.SimpleNamespace(path=types.SimpleNamespace(
            realpath=make_stub(trace, script, "realpath",
                               lambda path: {"path": path})))
        NS_CE["_python_environment_prefix"] = make_stub(
            trace, script, "python_environment_prefix",
            lambda path: {"path": path})
        ret = NS_CE["_uses_hermes_python_environment"](inp.get("python_path"))

    elif func == "strict_path_exists":
        path_d = types.SimpleNamespace(
            stat=make_stub(trace, script, "stat", {"path": inp.get("path", "")}))
        ret = NS_ST["_strict_path_exists"](path_d, inp.get("label", ""))

    elif func == "metadata":
        ret = NS_CUR["_merge_request_overrides"](
            inp.get("runtime_overrides"), inp.get("slot_extra_body"))

    else:
        raise RuntimeError(f"unknown func {func}")

    return ret, trace


def run_case(func, inp):
    trace = []
    try:
        ret, _ = drive(func, inp, trace)
    except BaseException as e:  # SystemExit included — fail closed, never die
        ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": norm(ret), "trace": norm(trace)}


def main():
    n = 0
    mismatches = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            print(f"FATAL: harness line not JSON: {e}")
            sys.exit(2)
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out") or {}
        if "error" in got:
            mismatches += 1
            print(f"MISMATCH [{n}] {func}: harness error {got['error']!r}")
            continue
        want = run_case(func, inp)
        g_ret, g_trace = got.get("ret"), got.get("trace")
        if g_ret != want["ret"] or g_trace != want["trace"]:
            mismatches += 1
            print(f"MISMATCH [{n}] {func}")
            print(f"  in:   {json.dumps(inp, ensure_ascii=False)}")
            if g_ret != want["ret"]:
                print(f"  ret want: {json.dumps(want['ret'], ensure_ascii=False)}")
                print(f"  ret got:  {json.dumps(g_ret, ensure_ascii=False)}")
            if g_trace != want["trace"]:
                print(f"  trace want: {json.dumps(want['trace'], ensure_ascii=False)}")
                print(f"  trace got:  {json.dumps(g_trace, ensure_ascii=False)}")
    print(f"ORACLE tail_revive3: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()