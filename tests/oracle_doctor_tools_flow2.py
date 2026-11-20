#!/usr/bin/env python3
"""Oracle for the doctor_tools.py REMAINDER FLOW port (src/cli/doctor_tools_flow2.c).

Drives the LIVE defs from hermes_cli/doctor_tools.py with SCRIPTED I/O: every
collaborator is a recording stub fed the same scripts the C harness feeds its
seams. Compares the ordered seam-name TRACE, the return (ret), the mutated
caller-held state, and captured report rows. Claimed siblings (check_bool,
_fail_and_issue, _pm_tool_path, _require, ...) are SEAMED; unclaimed functions
that the C port COMPOSES are driven through their real extracted bodies so the
inner seams fire identically.

Live defs are AST-extracted; the heavy quarry import chain is never imported.
The full py3.14 builtin exception lineage is exercised by scripted raises.
Fail-closed: a missing seam script raises RuntimeError("unscripted seam: X").
0 cases => exit 1. Summary: 'ORACLE doctor_tools_flow2: N cases, M mismatches'.
"""
from __future__ import annotations

import ast
import builtins
import contextlib
import copy
import __future__
import json as _REAL_JSON
import os
import sys
import types
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "doctor_tools.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
HCLI = QUARRY / "hermes_cli"
SRC = str(HCLI / "doctor_tools.py")

# func key -> live def name (all 17 UNCLAIMED defs ported by doctor_tools_flow2).
PY = {
    "doctor_tool": "_doctor_tool",
    "run_ok": "_run_ok",
    "doctor_web_capability_rows": "_doctor_web_capability_rows",
    "check_git_and_rg": "_check_git_and_rg",
    "check_docker_backend": "_check_docker_backend",
    "check_ssh_backend": "_check_ssh_backend",
    "check_daytona_backend": "_check_daytona_backend",
    "check_vercel_backend": "_check_vercel_backend",
    "check_plugin_backend": "_check_plugin_backend",
    "check_terminal_backend": "_check_terminal_backend",
    "check_chromium": "_check_chromium",
    "check_lightpanda": "_check_lightpanda",
    "check_node_and_browser": "_check_node_and_browser",
    "audit_one": "_audit_one",
    "check_npm_audit": "_check_npm_audit",
    "check_tool_availability": "_check_tool_availability",
    "warn_partially_stripped_toolsets": "_warn_partially_stripped_toolsets",
}

# Remainder functions the C port CALLS DIRECTLY (composes) rather than seams.
# When driving a func, these callees must stay REAL in the oracle namespace.
COMPOSED = {
    "check_git_and_rg": ["_doctor_tool"],
    "check_docker_backend": ["_run_ok"],
    "check_ssh_backend": ["_run_ok"],
    "check_terminal_backend": ["_check_docker_backend", "_check_ssh_backend",
                               "_check_daytona_backend", "_check_vercel_backend",
                               "_check_plugin_backend"],
    "check_node_and_browser": ["_check_chromium", "_check_lightpanda"],
    "check_npm_audit": ["_audit_one"],
    "check_tool_availability": ["_warn_partially_stripped_toolsets"],
    # doctor_web_capability_rows is SEAMED by check_tool_availability (not composed).
}
# Claimed siblings seamed by the C port -> (live name, seam name).
CLAIMED_SEAM = {
    "_safe_which": "safe_which",
    "_pm_tool_path": "pm_tool_path",
    "_pm_package_for_command": "pm_package_for_command",
    "_chromium_not_needed_reason": "chromium_not_needed_reason",
    "_doctor_tool_availability_detail": "doctor_tool_availability_detail",
    "_apply_doctor_tool_availability_overrides": "apply_tool_availability_overrides",
    "_missing_api_key_toolsets_for_summary": "missing_api_key_toolsets",
    "_is_kanban_worker_env_gate": "is_kanban_worker_env_gate",
    "_setup_gated": "setup_gated",
}
ANN_FLAG = __future__.annotations.compiler_flag


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "SystemExit":
        return SystemExit(spec.get("code", 1))
    if t == "TimeoutExpired":
        return TIMEOUT_EXC(msg)   # single shared class (see install_stubs)
    cls = getattr(builtins, t, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        try:
            return cls(msg)
        except TypeError:
            e = cls.__new__(cls); BaseException.__init__(e, msg); return e
    return type(t, (Exception,), {})(msg)


def exc_spec(e):
    spec = {"type": type(e).__name__, "message": str(e)}
    if isinstance(e, SystemExit):
        spec["code"] = e.code
    return spec


class PathDouble:
    """PROJECT_ROOT / tree double: `/` joins, `.exists()` is a seam."""
    def __init__(self, s): self._s = str(s)
    def __truediv__(self, other):
        return PathDouble(self._s.rstrip("/") + "/" + str(other))
    def exists(self):
        return CURRENT["ctx"].recorder("path_exists")(self._s)
    def __str__(self): return self._s
    def __fspath__(self): return self._s
    def __repr__(self): return "PathDouble(%r)" % self._s


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.stdout = []
        self.stderr = []
        self.rows = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})

    def peek(self, name):
        entry = self.io.get(name)
        if entry is None: return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            return entry[idx] if idx < len(entry) else None
        return entry

    def consume(self, name):
        if isinstance(self.io.get(name), list):
            self.cursors[name] = self.cursors.get(name, 0) + 1

    def recorder(self, name):
        ctx = self
        def call(*args, **kwargs):
            ctx.trace.append(name)
            entry = ctx.peek(name)
            if entry is None:
                raise RuntimeError("unscripted seam: " + name)
            ctx.consume(name)
            if isinstance(entry, dict):
                eff = entry.get("state")
                if isinstance(eff, dict): ctx.state.update(copy.deepcopy(eff))
                if isinstance(entry.get("raise"), dict):
                    raise make_exc(entry["raise"])
                ret = entry.get("ret")
            else:
                ret = entry
            return convert_seam_ret(name, copy.deepcopy(ret))
        return call


CURRENT = {"ctx": None}
# Single shared TimeoutExpired class: make_exc and the subprocess stub must
# raise/catch the SAME class object.
TIMEOUT_EXC = type("TimeoutExpired", (Exception,), {})


def convert_seam_ret(name, ret):
    ctx = CURRENT["ctx"]
    if name == "subprocess_run":
        if isinstance(ret, dict): return types.SimpleNamespace(**ret)
        return ret
    if name == "describe_vercel_auth":
        if isinstance(ret, dict): return types.SimpleNamespace(**ret)
        return ret
    if name in ("get_active_search_provider", "get_active_extract_provider"):
        if isinstance(ret, dict): return types.SimpleNamespace(**ret)
        return ret
    if name == "check_tool_availability":
        if isinstance(ret, dict):
            return (ret.get("available", []), ret.get("unavailable", []))
        return (None, None)
    if name in ("apply_tool_availability_overrides", "apply_doctor_tool_availability_overrides"):
        if isinstance(ret, dict):
            return (ret.get("available", []), ret.get("unavailable", []))
        return (None, None)
    if name == "resolve_whatsapp_bridge_dir":
        if isinstance(ret, str): return PathDouble(ret)
        return ret
    if name == "get_provider":
        if isinstance(ret, dict):
            p = types.SimpleNamespace(**ret)
            if not hasattr(p, "doctor_checks"):
                p.doctor_checks = ctx.recorder("doctor_checks")
            return p
        return ret
    return ret


def _mod(name, attrs=None, pkg=False):
    m = types.ModuleType(name)
    if pkg: m.__path__ = []
    for k, v in (attrs or {}).items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _exec_node(node, ns, src):
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
        node = copy.copy(node); node.decorator_list = []
    seg = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(seg)
    exec(compile(seg, src, "exec", flags=ANN_FLAG), ns)


def extract():
    mod = types.ModuleType("doctor_tools_flow2_oracle")
    sys.modules["doctor_tools_flow2_oracle"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "doctor_tools_flow2_oracle", "__future__": __future__,
               "os": os, "sys": sys, "json": _REAL_JSON, "Path": PurePosixPath})
    tree = ast.parse(Path(SRC).read_text(encoding="utf-8"))
    # functions first, then assigns/constants (two passes for default-arg refs).
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try: _exec_node(node, ns, SRC)
            except Exception: pass
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name): continue
            try: _exec_node(node, ns, SRC)
            except Exception: pass
    ns["_TOOLSET_SETUP_HINTS"] = {
        "image_gen": "(image generation unavailable — check the provider selection and its key or SDK with 'hermes tools')"
    }
    missing = [v for v in PY.values() if v not in ns]
    if missing:
        print("FATAL doctor_tools_flow2: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns


NS = extract()
REAL = {k: NS[k] for k in PY.values()}
CLAIMED_REAL = {k: NS[k] for k in ("_require", "_plural") if k in NS}
# Composed in C at EVERY call site (never a seam) -> always real in the oracle.
ALWAYS_REAL = {"_require", "_plural", "_run_ok"}
ALWAYS_REAL_FN = {k: NS[k] for k in ALWAYS_REAL if k in NS}


def proj(v):
    if isinstance(v, dict): return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)): return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)): return sorted(v)
    if isinstance(v, (PurePosixPath, Path)): return v.as_posix()
    if isinstance(v, PathDouble): return str(v)
    if isinstance(v, types.SimpleNamespace): return {k: proj(x) for k, x in vars(v).items()}
    if isinstance(v, BaseException): return exc_spec(v)
    return v


def row_stub(kind, seam):
    def call(text="", detail=""):
        ctx = CURRENT["ctx"]
        ctx.trace.append(seam)
        ctx.rows.append({"kind": kind, "text": text, "detail": detail})
        return None
    return call


def fail_and_issue_stub():
    def call(text, detail, fix, issues):
        ctx = CURRENT["ctx"]
        ctx.trace.append("fail_and_issue")
        issues.append(fix)
        return None
    return call


def make_f(ctx):
    return types.SimpleNamespace(
        issues=ctx.state.setdefault("issues", []),
        manual_issues=ctx.state.setdefault("manual_issues", []),
        fixed=0,
    )


def install_stubs(ctx, inp, func):
    ns = NS
    rec = ctx.recorder

    # default: all remainder functions are SEAMED under their live-name-minus-underscore
    for k, live in PY.items():
        ns[live] = rec(live.lstrip("_"))
    # claimed siblings seamed
    for live, seam in CLAIMED_SEAM.items():
        if live in ns: ns[live] = rec(seam)
    # claimed siblings composed inline by C -> real
    for live, fn in ALWAYS_REAL_FN.items():
        ns[live] = fn

    # remainder callees the C port composes -> real for this case
    for live in COMPOSED.get(func, []):
        if live in REAL: ns[live] = REAL[live]
    # the driven function itself is always real
    ns[PY[func]] = REAL[PY[func]]

    # globals / claimed collaborators
    ns["Path"] = PurePosixPath
    ns["os"] = types.SimpleNamespace(
        environ=types.SimpleNamespace(get=rec("environ_get")),
        getenv=rec("os_getenv"),
        path=types.SimpleNamespace(expanduser=rec("expanduser")))
    ns["sys"] = types.SimpleNamespace(path=[])
    ns["subprocess"] = types.SimpleNamespace(run=rec("subprocess_run"),
                                             TimeoutExpired=TIMEOUT_EXC)
    ns["importlib"] = types.SimpleNamespace(util=types.SimpleNamespace(find_spec=rec("find_spec")))
    ns["_system_package_install_cmd"] = rec("system_package_install_cmd")
    ns["_is_termux"] = rec("is_termux")
    ns["find_docker"] = rec("find_docker")
    ns["docker_runtime_name"] = rec("docker_runtime_name")
    ns["docker_runtime_start_hint"] = rec("docker_runtime_start_hint")
    ns["describe_vercel_auth"] = rec("describe_vercel_auth")
    ns["check_bool"] = rec("check_bool")
    ns["_fail_and_issue"] = fail_and_issue_stub()
    ns["doctor_check"] = lambda *a, **k: (lambda f: f)
    # report primitives (AUTO)
    for k, seam, kind in (("check_ok","check_ok","ok"), ("check_warn","check_warn","warn"),
                          ("check_fail","check_fail","fail"), ("check_info","check_info","info"),
                          ("_section","_section","section")):
        ns[k] = row_stub(kind, seam)
    # backend dispatch dict rebuilt against this case's stubs
    ns["_BACKEND_CHECKS"] = {
        "ssh": ns["_check_ssh_backend"],
        "daytona": ns["_check_daytona_backend"],
        "vercel_sandbox": ns["_check_vercel_backend"],
    }

    # in-function import stubs
    _mod("pm", pkg=True)
    pm = sys.modules["pm"]
    pm.installed_package = rec("installed_package")
    pm.registry = types.SimpleNamespace(all_packages=rec("all_packages"), get_package=rec("get_package"))
    pm.store = types.SimpleNamespace(current_target=rec("current_target"))
    pm.ensure = rec("pm_ensure")
    _mod("pm.packages", {"BinaryPackage": type("BinaryPackage", (), {}), "pnpm_env": rec("pnpm_env")})
    _mod("agent", pkg=True)
    _mod("agent.web_search_registry", {
        "get_active_search_provider": rec("get_active_search_provider"),
        "get_active_extract_provider": rec("get_active_extract_provider")})
    _mod("agent.terminal_env_registry", {"get_provider": rec("get_provider")})
    _mod("tools", pkg=True)
    _mod("tools.web_tools", {
        "_ensure_web_plugins_loaded": rec("ensure_web_plugins_loaded"),
        "_provider_is_ready": rec("_provider_is_ready")})
    _mod("tools.browser_lightpanda", {
        "LIGHTPANDA_INSTALL_HINT": "Install Lightpanda from https://lightpanda.io/docs/run-locally/installation/one-liner and make sure `lightpanda` is on PATH",
        "find_lightpanda_binary": rec("find_lightpanda_binary"),
        "lightpanda_engine_status": rec("lightpanda_engine_status"),
        "using_lightpanda_engine": rec("using_lightpanda_engine")})
    _mod("tools.terminal_tool_backends", {
        "_SUPPORTED_VERCEL_RUNTIMES": ["node20", "node22", "node24"]})
    _mod("daytona", {"Daytona": type("Daytona", (), {})})
    _mod("hermes_cli", pkg=True)
    _mod("hermes_cli.plugins", {"discover_plugins": rec("discover_plugins")})
    _mod("hermes_cli.config", {
        "cfg_get": rec("cfg_get"), "load_config": rec("load_config"),
        "load_config_readonly": rec("load_config_readonly")})
    _mod("hermes_cli.tools_config", {
        "_get_platform_tools": rec("get_platform_tools"),
        "toolsets_partially_stripped": rec("toolsets_partially_stripped")})
    _mod("hermes_cli.doctor", {"PROJECT_ROOT": PathDouble("/proj")})
    _mod("hermes_cli.browser_runtime", {
        "chromium_executable": rec("chromium_executable"),
        "chromium_missing_hint": rec("chromium_missing_hint")})
    _mod("hermes_constants", {
        "is_container": rec("is_container"), "is_termux": rec("is_termux"),
        "with_hermes_node_path": rec("with_hermes_node_path")})
    _mod("gateway", pkg=True)
    _mod("gateway.platforms", pkg=True)
    _mod("gateway.platforms.whatsapp_common", {
        "resolve_whatsapp_bridge_dir": rec("resolve_whatsapp_bridge_dir")})
    _mod("model_tools", {"check_tool_availability": rec("check_tool_availability")})
    # audit_one does `import json` in-function: intercept via sys.modules.
    sys.modules["json"] = types.SimpleNamespace(loads=rec("json_loads"))


def drive(func, inp, ctx):
    ns = NS; g = inp.get; f = make_f(ctx)
    if func == "doctor_tool": return ns["_doctor_tool"](g("name", ""))
    if func == "run_ok": return ns["_run_ok"](g("cmd", []) or [], g("timeout", 10))
    if func == "doctor_web_capability_rows": return ns["_doctor_web_capability_rows"]()
    if func == "check_git_and_rg": return ns["_check_git_and_rg"](g("should_fix", False), f)
    if func == "check_docker_backend":
        return ns["_check_docker_backend"](g("terminal_env", ""), g("running_in_container", False), ctx.state["issues"])
    if func == "check_ssh_backend": return ns["_check_ssh_backend"](ctx.state["issues"])
    if func == "check_daytona_backend": return ns["_check_daytona_backend"](ctx.state["issues"])
    if func == "check_vercel_backend": return ns["_check_vercel_backend"](ctx.state["issues"])
    if func == "check_plugin_backend": return ns["_check_plugin_backend"](g("terminal_env", ""), ctx.state["issues"])
    if func == "check_terminal_backend": return ns["_check_terminal_backend"](g("should_fix", False), f)
    if func == "check_chromium": return ns["_check_chromium"](g("should_fix", False))
    if func == "check_lightpanda": return ns["_check_lightpanda"]()
    if func == "check_node_and_browser": return ns["_check_node_and_browser"](g("should_fix", False), f)
    if func == "audit_one":
        return ns["_audit_one"](g("tool", ""), g("cwd", ""), g("label", ""),
                                g("audit_extra", []) or [], ctx.state["manual_issues"])
    if func == "check_npm_audit": return ns["_check_npm_audit"](g("should_fix", False), f)
    if func == "check_tool_availability": return ns["_check_tool_availability"](g("should_fix", False), f)
    if func == "warn_partially_stripped_toolsets": return ns["_warn_partially_stripped_toolsets"](f)
    raise KeyError(func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    install_stubs(ctx, inp, func)
    out = {"ret": None, "trace": ctx.trace}
    try:
        out["ret"] = drive(func, inp, ctx)
    except BaseException as e:
        out["ret"] = {"__raise__": exc_spec(e)}
    finally:
        sys.modules["json"] = _REAL_JSON
    out["ret"] = proj(out["ret"])
    out["state"] = {k: proj(ctx.state[k]) for k in (inp.get("state") or {}) if k in ctx.state}
    out["stdout"] = ctx.stdout
    out["stderr"] = ctx.stderr
    out["rows"] = ctx.rows
    return out


def main():
    cases = 0; mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"): continue
        try: c = _REAL_JSON.loads(line)
        except _REAL_JSON.JSONDecodeError: continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1; print(f"PYERR {func}: unknown func"); continue
        try: want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1; print(f"PYERR {func}: {type(e).__name__}: {e}"); continue
        got = c.get("out") or {}
        ok = (got.get("trace") == want.get("trace") and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state") and got.get("stdout") == want.get("stdout")
              and got.get("stderr") == want.get("stderr") and got.get("rows") == want.get("rows"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={_REAL_JSON.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={_REAL_JSON.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE doctor_tools_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
