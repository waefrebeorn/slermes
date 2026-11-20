#!/usr/bin/env python3
"""Oracle for the doctor_* sibling remainder FLOW port (src/cli/doctor_flow2.py).

Drives the LIVE defs from hermes_cli/doctor_tools.py + doctor_config.py +
doctor_platform.py with SCRIPTED I/O: every collaborator is a recording stub
fed the same scripts the C harness feeds its seams. Compares the ordered seam
name TRACE, the return (ret), the mutated caller-held state, and captured
report rows. Claimed siblings (check_bool, report primitives, warn_on_error,
Path stat/access, ...) are SEAMED. Live defs are AST-extracted; the heavy
quarry import chain is never imported. Full py3.14 builtin exception lineage
is exercised by scripted raises at except sites.

Fail-closed: a missing seam script raises RuntimeError("unscripted seam: X").
0 cases => exit 1. Summary: 'ORACLE doctor_flow2: N cases, M mismatches'.
"""
from __future__ import annotations

import ast
import builtins
import contextlib
import copy
import __future__
import json
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

# func key -> live def name (defining module noted in the C PoP).
PY = {
    "plural": "_plural",
    "setup_gated": "_setup_gated",
    "pm_tool_path": "_pm_tool_path",
    "pm_package_for_command": "_pm_package_for_command",
    "require": "_require",
    "chromium_not_needed_reason": "_chromium_not_needed_reason",
    "failing_boot_migration": "_failing_boot_migration",
    "drift_deprecations": "_drift_deprecations",
    "drift_structure": "_drift_structure",
    "provider_has_credentials": "_provider_has_credentials",
    "python_repair_hint": "_python_repair_hint",
    "unreadable_reason": "_unreadable_reason",
}
ANN_FLAG = __future__.annotations.compiler_flag


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "SystemExit":
        return SystemExit(spec.get("code", 1))
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


class FakePath:
    def __init__(self, s): self._s = str(s)
    def stat(self): return CURRENT["ctx"].recorder("path_stat")(self)
    def __fspath__(self): return self._s
    def __str__(self): return self._s


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
BINPKG = None  # placeholder BinaryPackage base class, built during extract()


class BinPkgDouble:
    """A BinaryPackage-materialized package double: ._rel is a recorder."""
    def __init__(self, ctx):
        self._rel = ctx.recorder("_rel")


class RegistryProxy:
    def __init__(self, ctx):
        self.ctx = ctx
        self.cache = None

    def get(self, name):
        if self.cache is None:
            r = self.ctx.recorder("provider_registry")()
            self.cache = r if isinstance(r, dict) else {}
        v = self.cache.get(name)
        if isinstance(v, dict):
            return types.SimpleNamespace(**v)
        return v


def convert_seam_ret(name, ret):
    ctx = CURRENT["ctx"]
    if name == "installed_package":
        if isinstance(ret, dict):
            b = ret.get("binary")
            return types.SimpleNamespace(binary=PurePosixPath(b) if isinstance(b, str) else None)
        return None
    if name == "get_package":
        if isinstance(ret, dict) and ret.get("binary"):
            o = BinPkgDouble(ctx)
            if BINPKG is not None: o.__class__ = type("BinPkg", (BINPKG,), {})
            return o
        return types.SimpleNamespace()  # not a BinaryPackage
    if name == "validate_config_structure":
        out = []
        for ci in (ret or []):
            out.append(types.SimpleNamespace(severity=ci.get("severity", ""),
                                             message=ci.get("message", ""),
                                             hint=ci.get("hint", "")))
        return out
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
    global BINPKG
    mod = types.ModuleType("doctor_flow2_oracle")
    sys.modules["doctor_flow2_oracle"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "doctor_flow2_oracle", "__future__": __future__,
               "os": os, "sys": sys, "json": json, "Path": PurePosixPath})
    BINPKG = type("BinaryPackage", (), {})
    modules = ["doctor_tools.py", "doctor_config.py", "doctor_platform.py"]
    trees = {}
    for m in modules:
        src = str(HCLI / m)
        trees[m] = (src, ast.parse(Path(src).read_text(encoding="utf-8")))
    for m in modules:
        src, tree = trees[m]
        for node in tree.body:
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                try: _exec_node(node, ns, src)
                except Exception: pass
    for m in modules:
        src, tree = trees[m]
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
                if not isinstance(tgt, ast.Name): continue
                try: _exec_node(node, ns, src)
                except Exception: pass
    ns["_TOOLSET_SETUP_HINTS"] = {"image_gen": "(image generation unavailable)"}
    missing = [v for v in PY.values() if v not in ns]
    if missing:
        print("FATAL doctor_flow2: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns


NS = extract()
REAL = {k: NS[k] for k in PY.values()}


def proj(v):
    if isinstance(v, dict): return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)): return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)): return sorted(v)
    if isinstance(v, (PurePosixPath, Path)): return v.as_posix()
    if isinstance(v, FakePath): return str(v)
    if isinstance(v, BaseException): return exc_spec(v)
    return v


def row_stub(kind, seam):
    def call(text="", detail=""):
        ctx = CURRENT["ctx"]
        ctx.trace.append(seam)
        ctx.rows.append({"kind": kind, "text": text, "detail": detail})
        return None
    return call


def install_stubs(ctx, inp):
    ns = NS
    rec = ctx.recorder
    for k, v in REAL.items(): ns[k] = v
    ns["Path"] = PurePosixPath
    ns["os"] = types.SimpleNamespace(
        environ=types.SimpleNamespace(get=rec("environ_get")),
        getenv=rec("os_getenv"), access=rec("os_access"), R_OK=4)
    # claimed collaborators -> seams
    ns["check_bool"] = rec("check_bool")
    ns["report_deprecated_config_and_env"] = rec("report_deprecated_config_and_env")
    ns["is_nix_install_method"] = rec("is_nix_install_method")
    ns["recommended_update_command_for_method"] = rec("recommended_update_command_for_method")
    # report primitives (AUTO)
    for k, seam, kind in (("check_ok","check_ok","ok"), ("check_warn","check_warn","warn"),
                          ("check_fail","check_fail","fail"), ("check_info","check_info","info"),
                          ("_section","_section","section")):
        ns[k] = row_stub(kind, seam)
    # warn_on_error: claimed sibling CM, swallows Exception, BaseException passes
    def warn_on_error(txt="", detail="({e})", report=None):
        @contextlib.contextmanager
        def cm():
            try: yield
            except Exception: pass
        return cm()
    ns["warn_on_error"] = warn_on_error

    # in-function import stubs
    _mod("hermes_cli", pkg=True)
    _mod("pm", pkg=True)
    pm = sys.modules["pm"]
    pm.installed_package = rec("installed_package")
    pm.registry = types.SimpleNamespace(all_packages=rec("all_packages"), get_package=rec("get_package"))
    pm.store = types.SimpleNamespace(current_target=rec("current_target"))
    _mod("pm.packages", {"BinaryPackage": BINPKG})
    _mod("pm.paths", {"install_root": rec("install_root")})
    cfg = _mod("hermes_cli.config", {
        "cfg_get": rec("cfg_get"),
        "get_env_value": rec("get_env_value"),
        "load_config_readonly": rec("load_config_readonly"),
        "load_env": rec("load_env"),
        "read_user_config_raw": rec("read_user_config_raw"),
        "detect_install_method": rec("detect_install_method"),
        "validate_config_structure": rec("validate_config_structure"),
    })
    _mod("hermes_cli.doctor", {"PROJECT_ROOT": FakePath("/proj")})
    _mod("hermes_cli.boot_bootstrap", {
        "owed_config_migration": rec("owed_config_migration"),
        "read_last_known": rec("read_last_known"),
        "record_path": rec("record_path"),
    })
    _mod("hermes_cli.auth", {"PROVIDER_REGISTRY": RegistryProxy(ctx), "get_auth_status": rec("get_auth_status")})


def drive(func, inp, ctx):
    ns = NS; g = inp.get
    if func == "plural": return ns["_plural"](g("n", 0))
    if func == "setup_gated": return ns["_setup_gated"](g("item") or {})
    if func == "pm_tool_path": return ns["_pm_tool_path"](g("name", ""))
    if func == "pm_package_for_command": return ns["_pm_package_for_command"](g("command", ""))
    if func == "require":
        issues = ctx.state.setdefault("issues", [])
        return ns["_require"](g("cond"), g("ok"), g("bad"), g("issue", ""), issues)
    if func == "chromium_not_needed_reason": return ns["_chromium_not_needed_reason"]()
    if func == "failing_boot_migration": return ns["_failing_boot_migration"]()
    if func == "drift_deprecations": return ns["_drift_deprecations"](None, g("should_fix", False), g("config_path"))
    if func == "drift_structure":
        issues = ctx.state.setdefault("issues", [])
        f = types.SimpleNamespace(issues=issues, manual_issues=[], fixed=0)
        return ns["_drift_structure"](f, g("should_fix", False), g("config_path"))
    if func == "provider_has_credentials": return ns["_provider_has_credentials"](g("provider", ""))
    if func == "python_repair_hint": return ns["_python_repair_hint"]()
    if func == "unreadable_reason": return ns["_unreadable_reason"](FakePath(g("path", "")))
    raise KeyError(func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    install_stubs(ctx, inp)
    out = {"ret": None, "trace": ctx.trace}
    try:
        out["ret"] = drive(func, inp, ctx)
    except BaseException as e:
        out["ret"] = {"__raise__": exc_spec(e)}
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
        try: c = json.loads(line)
        except json.JSONDecodeError: continue
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
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE doctor_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
