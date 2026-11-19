#!/usr/bin/env python3
"""Oracle for the doctor-diagnostic UNCLAIMED FLOW port (src/cli/doctor_flow.c).

Drives the LIVE defs from hermes_cli/doctor.py + doctor_report.py + doctor_live.py
+ doctor_state.py + doctor_config.py with SCRIPTED I/O: every collaborator is a
recording stub fed the same scripts the C harness feeds its seams. Compares the
ordered seam-name TRACE, the return (ret), the mutated caller-held Finding mirror
(state), captured print output (stdout/stderr) and report-primitive rows (rows).

The ported siblings (_mark, check_bool, warn_on_error, ensure_dir, Finding.merge,
_login_row, _print_summary, _ack_advisory, _check_auth_providers,
_check_api_connectivity, run_doctor, maybe_run_live_checks, _keyed_probe,
_probe_audio, _bits, host_gateway_note, _session_count, _endpoint_url,
_check_retired_session_reset, _check_xai_retirement) stay REAL so nested decision
logic is exercised. CLAIMED siblings (check_ok/check_warn/check_fail/check_info/
_section, color, print, _safe_which, build_probes, run_probes,
_has_healthy_oauth_fallback_for_apikey_provider, _http_get, _classify_http,
run_live_checks, open_store_admin) are SEAMED — the report primitives + print +
color are AUTO cosmetic seams; the rest are scripted + fail-closed.

In-function imports are modeled by import-guard sys.modules stubs that fire ONE
import seam token on first attribute access (matching the C import seam), or
raise an import_raise spec WITHOUT a token (the C __untrace__ retraction).

Live defs are AST-extracted (ClassDef + FunctionDef + Assign exec, future
annotations). The heavy quarry import chain is never imported. Fail-closed: a
missing seam script raises RuntimeError("unscripted seam: <name>"); 0 cases =>
exit 1. Summary: 'ORACLE doctor_flow: N cases, M mismatches'.
"""
import ast
import builtins
import copy
import __future__
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "doctor.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
HCLI = QUARRY / "hermes_cli"

# func key -> live def name (defining module noted in the C PoP).
PY = {
    "mark": "_mark",
    "check_bool": "check_bool",
    "finding_merge": "merge",
    "ensure_dir": "ensure_dir",
    "warn_on_error": "warn_on_error",
    "login_row": "_login_row",
    "print_summary": "_print_summary",
    "ack_advisory": "_ack_advisory",
    "check_auth_providers": "_check_auth_providers",
    "check_api_connectivity": "_check_api_connectivity",
    "run_doctor": "run_doctor",
    "maybe_run_live_checks": "maybe_run_live_checks",
    "keyed_probe": "_keyed_probe",
    "probe_audio": "_probe_audio",
    "bits": "_bits",
    "host_gateway_note": "host_gateway_note",
    "session_count": "_session_count",
    "endpoint_url": "_endpoint_url",
    "check_retired_session_reset": "_check_retired_session_reset",
    "check_xai_retirement": "_check_xai_retirement",
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
        return cls(msg)
    return type(t, (Exception,), {})(msg)


def exc_spec(e):
    spec = {"type": type(e).__name__, "message": str(e)}
    if isinstance(e, SystemExit):
        spec["code"] = e.code
    return spec


class FakePath:
    def __init__(self, s):
        self._s = str(s)

    def exists(self):
        return CURRENT["ctx"].recorder("path_exists")(self)

    def mkdir(self, parents=False, exist_ok=False):
        return CURRENT["ctx"].recorder("path_mkdir")(self, parents=parents, exist_ok=exist_ok)

    def __fspath__(self):
        return self._s

    def __str__(self):
        return self._s


class SilentCM:
    def __enter__(self):
        return None

    def __exit__(self, *a):
        return False


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.stdout = []
        self.stderr = []
        self.rows = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.import_fired = set()
        self.state = copy.deepcopy(inp.get("state", {}) or {})
        self.findings = []

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
                if isinstance(eff, dict):
                    ctx.state.update(copy.deepcopy(eff))
                if isinstance(entry.get("raise"), dict):
                    raise make_exc(entry["raise"])
                ret = entry.get("ret")
            else:
                ret = entry
            return convert_seam_ret(name, ret)
        return call


CURRENT = {"ctx": None}


def convert_seam_ret(name, ret):
    ctx = CURRENT["ctx"]
    if name == "run_probes":
        out = []
        for r in (ret or []):
            out.append(types.SimpleNamespace(
                label=r.get("label", ""),
                lines=[tuple(l) for l in r.get("lines", [])],
                issues=list(r.get("issues", []))))
        return out
    if name == "_classify_http":
        if isinstance(ret, dict):
            P = REAL["ProbeResult"]
            return P(name=ret.get("name", ""), status=ret.get("status", ""), detail=ret.get("detail", ""))
        return ret
    if name == "host_gateway_topology":
        if ret is None:
            return None
        return types.SimpleNamespace(describe=ctx.recorder("topology_describe"))
    if name == "open_store_admin":
        return types.SimpleNamespace(session_row_count=ctx.recorder("session_row_count"))
    if name == "retired_reset_policy":
        return tuple(ret) if isinstance(ret, list) else ret
    return ret


def make_import_mod(modname, import_seam, name_map):
    """import_seam: a str, or a dict {attr_name: seam} for multi-import modules."""
    m = types.ModuleType(modname)

    def getattr_(name):
        ctx = CURRENT["ctx"]
        seam = import_seam.get(name) if isinstance(import_seam, dict) else import_seam
        if seam is None:
            raise AttributeError(name)
        entry = ctx.peek(seam)
        if entry is None:
            ctx.trace.append(seam)
            raise RuntimeError("unscripted seam: " + seam)
        if isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict):
            ctx.consume(seam)
            raise make_exc(entry["import_raise"])   # no token (C __untrace__)
        if seam not in ctx.import_fired:
            ctx.trace.append(seam)
            ctx.import_fired.add(seam)
            ctx.consume(seam)
        if name in name_map:
            return name_map[name]
        raise AttributeError(name)

    m.__getattr__ = getattr_
    return m


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def mat_finding(d):
    d = d or {}
    F = REAL["Finding"]
    return F(issues=list(d.get("issues", [])),
             manual_issues=list(d.get("manual_issues", [])),
             fixed=d.get("fixed", 0))


def install_stubs(ctx, inp):
    ns = NS
    rec = ctx.recorder
    # restore every ported REAL def (a prior case may have left a recorder).
    for k, v in REAL.items():
        ns[k] = v

    def print_stub(*a, file=None, **k):
        ctx.trace.append("print")
        text = a[0] if a else ""
        (ctx.stderr if file is sys.stderr else ctx.stdout).append(text)
        return None
    ns["print"] = print_stub

    def color_stub(*a, **k):
        ctx.trace.append("color")
        return a[0] if a else ""
    ns["color"] = color_stub

    def row_stub(kind, seam):
        def call(text, detail=""):
            ctx.trace.append(seam)
            ctx.rows.append({"kind": kind, "text": text, "detail": detail})
            return None
        return call
    ns["check_ok"] = row_stub("ok", "check_ok")
    ns["check_warn"] = row_stub("warn", "check_warn")
    ns["check_fail"] = row_stub("fail", "check_fail")
    # warn_on_error's DEFAULT report=check_warn was bound at def-exec time to the
    # real _mark lambda; re-point it at the AUTO row stub so the seam abstraction
    # is consistent with check_bool's call-time check_warn resolution.
    wo = ns.get("warn_on_error")
    if wo is not None and hasattr(wo, "__wrapped__"):
        d = list(wo.__wrapped__.__defaults__ or ())
        if len(d) >= 2:
            d[-1] = ns["check_warn"]
            wo.__wrapped__.__defaults__ = tuple(d)
    ns["check_info"] = lambda text: (ctx.trace.append("check_info"),
                                     ctx.rows.append({"kind": "info", "text": text, "detail": ""}))[0]
    ns["_section"] = lambda title: (ctx.trace.append("_section"),
                                    ctx.rows.append({"kind": "section", "text": title, "detail": ""}))[0]
    AUTO_ROW = {"check_ok": ns["check_ok"], "check_warn": ns["check_warn"],
                "check_fail": ns["check_fail"], "check_info": ns["check_info"]}
    ctx.auto_row = AUTO_ROW

    ns["os"] = types.SimpleNamespace(
        environ=types.SimpleNamespace(setdefault=rec("os_environ_setdefault")),
        getenv=rec("os_getenv"))
    ns["sys"] = sys   # sys.exit raises SystemExit naturally (no seam)
    ns["_DHH"] = inp.get("dhh", "")
    ns["MIGRATION_GUIDE_URL"] = inp.get("migration_url", "")

    for name in ("_safe_which", "build_probes", "run_probes",
                 "_has_healthy_oauth_fallback_for_apikey_provider",
                 "_http_get", "_classify_http", "run_live_checks",
                 "ack_advisory", "load_config", "find_retired_xai_refs",
                 "format_issue", "load_user_config_effective",
                 "retired_reset_policy", "reset_plugin_enabled", "format_notice",
                 "host_gateway_topology", "open_store_admin"):
        ns[name] = rec(name)

    # ── in-function import guards (fire one import token on first access) ──
    _pkg("hermes_cli")
    _pkg("gateway")
    adv_ids = inp.get("advisory_ids", []) or []
    advisories = [types.SimpleNamespace(id=x) for x in adv_ids]
    auth_map = {
        "get_nous_auth_status_local": rec("get_nous_auth_status_local"),
        "get_codex_auth_status": rec("get_codex_auth_status"),
        "get_minimax_oauth_auth_status": rec("get_minimax_oauth_auth_status"),
        "get_xai_oauth_auth_status": rec("get_xai_oauth_auth_status"),
    }
    auth_seam = {
        "get_nous_auth_status_local": "auth_import",
        "get_codex_auth_status": "auth_import",
        "get_minimax_oauth_auth_status": "auth_import",
        "get_xai_oauth_auth_status": "xai_import",
    }
    sys.modules["hermes_cli.auth"] = make_import_mod("hermes_cli.auth", auth_seam, auth_map)
    sys.modules["hermes_cli.security_advisories"] = make_import_mod(
        "hermes_cli.security_advisories", "advisories_import",
        {"ADVISORIES": advisories, "ack_advisory": rec("ack_advisory")})
    sys.modules["gateway.host_topology"] = make_import_mod(
        "gateway.host_topology", "host_topology_import",
        {"host_gateway_topology": rec("host_gateway_topology")})
    sys.modules["hermes_cli.config_effective"] = make_import_mod(
        "hermes_cli.config_effective", "config_effective_import",
        {"load_user_config_effective": rec("load_user_config_effective")})
    sys.modules["hermes_cli.session_reset_retirement"] = make_import_mod(
        "hermes_cli.session_reset_retirement", "session_reset_import",
        {"format_notice": rec("format_notice"),
         "reset_plugin_enabled": rec("reset_plugin_enabled"),
         "retired_reset_policy": rec("retired_reset_policy")})
    sys.modules["hermes_cli.config"] = make_import_mod(
        "hermes_cli.config", "config_import", {"load_config": rec("load_config")})
    sys.modules["hermes_cli.xai_retirement"] = make_import_mod(
        "hermes_cli.xai_retirement", "xai_retirement_import",
        {"MIGRATION_GUIDE_URL": inp.get("migration_url", ""),
         "find_retired_xai_refs": rec("find_retired_xai_refs"),
         "format_issue": rec("format_issue")})
    sys.modules["hermes_cli.doctor_live"] = make_import_mod(
        "hermes_cli.doctor_live", "live_import",
        {"maybe_run_live_checks": rec("maybe_run_live_checks")})
    sys.modules["hermes_cli.left_core_migration"] = make_import_mod(
        "hermes_cli.left_core_migration", "installs_held",
        {"installs_held": lambda: SilentCM()})


def proj(v):
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(v)
    if isinstance(v, FakePath):
        return str(v)
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, BaseException):
        return exc_spec(v)
    if isinstance(v, REAL["Finding"]):
        return {"issues": proj(v.issues), "manual_issues": proj(v.manual_issues), "fixed": v.fixed}
    if isinstance(v, REAL["ProbeResult"]):
        return {"name": v.name, "status": v.status, "detail": v.detail}
    return v


def _exec_node(node, ns, src):
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
        node = copy.copy(node)
        node.decorator_list = []   # strip @doctor_check / @contextmanager
    seg = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(seg)
    exec(compile(seg, src, "exec", flags=ANN_FLAG), ns)


def extract():
    from dataclasses import dataclass, field
    # py3.14 dataclasses needs sys.modules[cls.__module__]; exec into a
    # registered module's __dict__ so Finding/ProbeResult resolve.
    mod = types.ModuleType("doctor_flow_oracle")
    sys.modules["doctor_flow_oracle"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "doctor_flow_oracle", "dataclass": dataclass, "field": field,
               "os": os, "sys": sys, "json": json, "Path": Path})
    # Colors (real ANSI constant class; color() itself is seamed).
    col_ns = {"os": os, "sys": sys}
    exec(compile((HCLI / "colors.py").read_text(encoding="utf-8"), "colors.py", "exec"), col_ns)
    ns["Colors"] = col_ns["Colors"]

    modules = ["doctor_report.py", "doctor_live.py", "doctor.py",
               "doctor_state.py", "doctor_config.py"]
    trees = {}
    for mod in modules:
        src = str(HCLI / mod)
        trees[mod] = (src, ast.parse(Path(src).read_text(encoding="utf-8")))
    # ClassDefs first (Finding, ProbeResult), then FunctionDefs, then Assigns.
    for mod in modules:
        src, tree = trees[mod]
        for node in tree.body:
            if isinstance(node, ast.ClassDef):
                try:
                    _exec_node(node, ns, src)
                except Exception:
                    pass
    for mod in modules:
        src, tree = trees[mod]
        for node in tree.body:
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                try:
                    _exec_node(node, ns, src)
                except Exception:
                    pass
    for mod in modules:
        src, tree = trees[mod]
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
                if isinstance(tgt, (ast.Tuple, ast.List)):
                    if not all(isinstance(e, ast.Name) for e in tgt.elts):
                        continue
                elif not isinstance(tgt, ast.Name):
                    continue
                try:
                    _exec_node(node, ns, src)
                except Exception:
                    pass
    # second function pass: defs whose DEFAULT ARGS reference module constants
    # (warn_on_error(report=check_warn)) need the Assign pass bound first.
    for mod in modules:
        src, tree = trees[mod]
        for node in tree.body:
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                try:
                    _exec_node(node, ns, src)
                except Exception:
                    pass
    # re-wrap contextmanager generators stripped by extraction
    import contextlib
    if "warn_on_error" in ns and isinstance(ns["warn_on_error"], types.FunctionType):
        ns["warn_on_error"] = contextlib.contextmanager(ns["warn_on_error"])
    missing = [v for v in PY.values() if v not in ns and v != "merge"]
    if "Finding" not in ns or not hasattr(ns["Finding"], "merge"):
        missing.append("Finding.merge")
    if "ProbeResult" not in ns:
        missing.append("ProbeResult")
    for sib in ("check_bool", "_mark", "ensure_dir", "Colors", "_LOCAL_AUDIO_PROVIDERS", "_AUDIO_PROBES"):
        if sib not in ns:
            missing.append(sib)
    if missing:
        print("FATAL doctor_flow: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns


NS = extract()
REAL = {k: NS[k] for k in PY.values() if k in NS and k != "merge"}
REAL["Finding"] = NS["Finding"]
REAL["ProbeResult"] = NS["ProbeResult"]
REAL["check_bool"] = NS["check_bool"]
REAL["_mark"] = NS["_mark"]
REAL["ensure_dir"] = NS["ensure_dir"]
REAL["warn_on_error"] = NS["warn_on_error"]


def drive(func, inp, ctx):
    ns = NS
    g = inp.get
    if func == "mark":
        fn = ns["_mark"](g("glyph", ""), g("col", ""))
        return fn(g("text", ""), g("detail", ""))
    if func == "check_bool":
        return ns["check_bool"](g("cond"), g("ok"), g("bad"), fail=g("fail", False))
    if func == "finding_merge":
        self = mat_finding(ctx.state.get("finding"))
        other = mat_finding(inp.get("other"))
        self.merge(other)
        ctx.state["finding"] = self
        return None
    if func == "ensure_dir":
        f = mat_finding(ctx.state.get("finding"))
        ns["ensure_dir"](f, g("should_fix", False), FakePath(g("path", "")),
                         g("exists_msg", ""), g("created_msg", ""), g("missing_msg", ""))
        ctx.state["finding"] = f
        return None
    if func == "warn_on_error":
        report = ctx.auto_row[g("report", "check_warn")]
        body = ctx.recorder(g("body", "body"))
        with ns["warn_on_error"](g("text", ""), g("detail", "({e})"), report=report):
            body()
        return None
    if func == "login_row":
        return ns["_login_row"](g("label", ""), g("status") or {},
                                g("ok_detail", "(logged in)"), show_error=g("show_error", False))
    if func == "print_summary":
        total = mat_finding(ctx.state.get("finding"))
        ns["_print_summary"](g("should_fix", False), total)
        return None
    if func == "ack_advisory":
        return ns["_ack_advisory"](g("ack_target", ""))
    if func == "check_auth_providers":
        f = mat_finding(ctx.state.get("finding"))
        ns["_check_auth_providers"](g("should_fix", False), f)
        ctx.state["finding"] = f
        return None
    if func == "check_api_connectivity":
        f = mat_finding(ctx.state.get("finding"))
        ns["_check_api_connectivity"](g("should_fix", False), f)
        ctx.state["finding"] = f
        return None
    if func == "run_doctor":
        # The C iterates the fixture check list; bind DOCTOR_CHECKS to recording
        # check seams that materialize a Finding from the scripted ret.
        def mk_check(nm):
            def call(should_fix):
                ctx.trace.append(nm)
                entry = ctx.peek(nm)
                if entry is None:
                    raise RuntimeError("unscripted seam: " + nm)
                ctx.consume(nm)
                if isinstance(entry, dict):
                    if isinstance(entry.get("raise"), dict):
                        raise make_exc(entry["raise"])
                    ret = entry.get("ret")
                else:
                    ret = entry
                return mat_finding(ret)
            return call
        ns["DOCTOR_CHECKS"] = tuple((ck.get("title"), mk_check(ck.get("name", "")))
                                    for ck in (inp.get("checks") or []))
        captured = []

        class FindingCap(REAL["Finding"]):
            def __init__(self, *a, **k):
                super().__init__(*a, **k)
                captured.append(self)
        ns["Finding"] = FindingCap
        try:
            args = types.SimpleNamespace(**(inp.get("args") or {}))
            ret = ns["run_doctor"](args)
        finally:
            ns["Finding"] = REAL["Finding"]
        if captured:
            ctx.state["finding"] = captured[0]
        return ret
    if func == "maybe_run_live_checks":
        issues = ctx.state.setdefault("live_issues", [])
        args = types.SimpleNamespace(**(inp.get("args") or {}))
        return ns["maybe_run_live_checks"](args, issues)
    if func == "keyed_probe":
        return ns["_keyed_probe"](g("name", ""), g("url", ""), g("env_var", ""),
                                  g("scheme", ""), g("timeout", 10.0))
    if func == "probe_audio":
        return ns["_probe_audio"](g("kind", ""), g("config") or {}, g("timeout", 10.0))
    if func == "bits":
        pairs = [(p.get("value"), ctx.recorder(p.get("fmt", ""))) for p in (inp.get("pairs") or [])]
        return ns["_bits"](*pairs)
    if func == "host_gateway_note":
        return ns["host_gateway_note"]()
    if func == "session_count":
        return ns["_session_count"](FakePath(g("path", "")))
    if func == "endpoint_url":
        return ns["_endpoint_url"](g("entry") or {})
    if func == "check_retired_session_reset":
        f = mat_finding(ctx.state.get("finding"))
        ns["_check_retired_session_reset"](g("should_fix", False), f)
        ctx.state["finding"] = f
        return None
    if func == "check_xai_retirement":
        f = mat_finding(ctx.state.get("finding"))
        ns["_check_xai_retirement"](g("should_fix", False), f)
        ctx.state["finding"] = f
        return None
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
              and got.get("stderr") == want.get("stderr")
              and got.get("rows") == want.get("rows"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE doctor_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())