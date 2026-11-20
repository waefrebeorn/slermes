#!/usr/bin/env python3
"""Oracle for gateway_enroll_flow.c — hermes_cli/gateway_enroll.py FLOW +
the UNCLAIMED hermes_cli/gateway_multiplex_mode.py leaf cluster (LIVE
authority: the quarry gateway_enroll.py / gateway_multiplex_mode.py).

Drives the LIVE functions with SCRIPTED I/O: the 21 targets are AST-extracted
from the two quarry modules, the MultiplexDecision dataclass is exec'd, module
constants seeded (Assign AND AnnAssign, re-seeded after the class binds), and
every env/config read, foreign-module call, print, sys.exit, logger verb and
Path marker op replaced by a recording stub returning the SAME scripted
results the C harness feeds its universal seam. The claimed gateway_enroll
siblings are NOT re-ported; their multiplex consult
(default_gateway_multiplexes) is one of the targets here. Internal
composition (the ported targets calling each other, the real dataclass field
defaults) runs REAL on both sides. Compares the ordered DECISION SEQUENCE +
return + mutated state (config object / _pending_decision list). Per-case
BaseException catch (SystemExit escapes except Exception and is compared as a
raise spec; scripted specs round-trip VERBATIM by identity).

Input: stdin JSON lines from t_port_gateway_enroll_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":...}}
Summary: ORACLE gateway_enroll_flow: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.

Fixture domain notes (kept identical on both sides):
- paths are non-empty absolute POSIX strings; PathDouble normalizes like
  Path.as_posix; path params are passed RAW so Path() conversions happen
  inside the live body at the live statement order;
- config doubles are dict|null; decision dicts carry enabled+source, reason
  defaults to ""; plan dicts carry standalone_secondaries ([{name,pid}]),
  blocked, blockers — missing keys raise AttributeError on the
  SimpleNamespace double exactly as live; pids int literals or null;
- profiles_to_serve returns [[name,home],...] or null; _fail lines is an
  array (possibly empty); _box bodies are lists of str;
- logger args project exception instances to str(exc); repr(in_file) is
  modelled for null/bool/int/simple-ASCII str only;
- a MISSING seam script returns None on both sides (the harness script_call
  yields a NULL pointer); the oracle is fail-closed at the SUMMARY level
  (0 cases / missing defs -> exit 2) and every seam raise propagates.
"""
import ast
import builtins
import dataclasses
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "ge2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC_ENROLL = os.path.join(QUARRY, "hermes_cli", "gateway_enroll.py")
SRC_MULTIPLEX = os.path.join(QUARRY, "hermes_cli", "gateway_multiplex_mode.py")

PY = {
    "fail": "_fail",
    "explicit_multiplex_flag": "explicit_multiplex_flag",
    "settled_explicit_flag": "settled_explicit_flag",
    "host_serves_named_profile": "host_serves_named_profile",
    "default_gateway_multiplexes": "default_gateway_multiplexes",
    "standalone_launcher": "_standalone_launcher",
    "standalone_launcher_decision": "standalone_launcher_decision",
    "implicit_multiplex_blocker": "implicit_multiplex_blocker",
    "default_profile_home": "_default_profile_home",
    "persist_resolved_default": "persist_resolved_default",
    "retired_opt_out_notice_lines": "retired_opt_out_notice_lines",
    "consume_rewritten_notice": "consume_rewritten_notice",
    "box": "_box",
    "resolve_multiplex_mode": "resolve_multiplex_mode",
    "record_multiplex_decision": "record_multiplex_decision",
    "publish_pending_multiplex_decision": "publish_pending_multiplex_decision",
    "log_multiplex_decision": "log_multiplex_decision",
    "unserved_profiles": "unserved_profiles",
    "standalone_warning_lines": "standalone_warning_lines",
    "recorded_standalone_reason": "recorded_standalone_reason",
    "recorded_standalone_warning_lines": "recorded_standalone_warning_lines",
}
WANTED = set(PY.values())
CLASSES = ["MultiplexDecision"]
CONSTS = ["SINGLE_PROFILE_REASON", "STANDALONE_PROFILE_REASON",
          "RETIRED_OPT_OUT_REASON", "REWRITTEN_MARKER_NAME", "logger"]
CONFIG_FUNCS = {"standalone_launcher_decision", "resolve_multiplex_mode"}
PENDING_FUNCS = {"record_multiplex_decision", "publish_pending_multiplex_decision",
                 "log_multiplex_decision"}
DECISION_FUNCS = {"standalone_launcher_decision", "resolve_multiplex_mode"}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": [], "stcfg": None}
NS_DEC = None
MIGRATE_COMMAND = "hermes gateway migrate --multiplex"


def _exc_class(name):
    cls = getattr(builtins, name, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls
    return type(name, (Exception,), {})


def make_exc(spec):
    cls = _exc_class(spec["__raise__"])
    msg = spec.get("msg", "")
    if cls in (UnicodeDecodeError, UnicodeEncodeError, UnicodeTranslateError):
        # multi-arg constructors: bypass __init__ (py3.14-safe form)
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


class Raise:
    def __init__(self, spec):
        self.spec = spec
        self.exc = make_exc(spec)


# ── doubles ──

class PathDouble:
    def __init__(self, p):
        if isinstance(p, PathDouble):
            s = p.s
        elif isinstance(p, str):
            s = p
        elif p is None:
            raise TypeError("argument should be a str or an os.PathLike object "
                            "where __fspath__ returns a str, not 'NoneType'")
        else:
            raise TypeError("argument should be a str or an os.PathLike object "
                            "where __fspath__ returns a str, not '%s'" % type(p).__name__)
        s = s.replace("\\", "/")
        while "//" in s:
            s = s.replace("//", "/")
        while len(s) > 1 and s.endswith("/"):
            s = s[:-1]
        self.s = s

    def __truediv__(self, name):
        n = str(name)
        base = self.s
        joined = base + n if base.endswith("/") else base + "/" + n
        return PathDouble(joined)

    def exists(self):
        return seam("marker_exists", {"path": self.s})

    def unlink(self):
        return seam("marker_unlink", {"path": self.s})

    def write_text(self, text, encoding=None):
        return seam("marker_write_text",
                    {"path": self.s, "text": text, "encoding": encoding})

    def as_posix(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __str__(self):
        return self.s

    def __repr__(self):
        return "PathDouble(%r)" % self.s

    def __eq__(self, other):
        if not isinstance(other, PathDouble):
            return NotImplemented
        return self.s.lower() == other.s.lower()

    def __hash__(self):
        return hash(self.s.lower())


def posix(p):
    if p is None:
        return None
    if isinstance(p, (PathDouble, Path)):
        return p.as_posix()
    return str(p).replace("\\", "/")


def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (Path, PathDouble)):
        return o.as_posix()
    if isinstance(o, BaseException):
        return str(o)
    if dataclasses.is_dataclass(o) and not isinstance(o, type):
        return {f.name: norm(getattr(o, f.name)) for f in dataclasses.fields(o)}
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in o.__dict__.items()}
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    return str(o)


def seam(name, args):
    args = {k: norm(v) for k, v in args.items() if v is not None}
    sc = CUR["scripts"].get(name)
    if sc is None:
        r = None
    elif isinstance(sc, list):
        i = CUR["idx"].get(name, 0)
        CUR["idx"][name] = i + 1
        r = sc[min(i, len(sc) - 1)] if sc else None
    else:
        r = sc
    r = json.loads(json.dumps(r))
    # Snapshot ret at call time (the C harness records a json_copy snapshot).
    CUR["trace"].append({"ev": name, "args": args, "ret": json.loads(json.dumps(r))})
    if isinstance(r, dict) and "__raise__" in r:
        rr = Raise(r)
        CUR["raised"].append(rr)
        raise rr.exc
    return r


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── extraction ──

def _assign_pass(tree, ns, src):
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not (node.value and isinstance(node.target, ast.Name)):
                continue
        else:
            continue
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec"), ns)
        except Exception:
            pass


def extract():
    import logging
    from dataclasses import dataclass, field
    from typing import Optional

    mod = types.ModuleType("gateway_enroll_flow_extracted")
    sys.modules["gateway_enroll_flow_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "gateway_enroll_flow_extracted",
        "json": json, "os": os, "sys": sys, "logging": logging, "types": types,
        "Path": PathDouble, "dataclass": dataclass, "field": field,
        "Optional": Optional,
        "SimpleNamespace": types.SimpleNamespace,
    })
    import __future__
    flag = __future__.annotations.compiler_flag
    trees = []
    for src in (SRC_ENROLL, SRC_MULTIPLEX):
        text = open(src, encoding="utf-8").read()
        trees.append((ast.parse(text), src))
    # constants first (logger + reason strings), then the dataclass, then
    # constants AGAIN (_pending_decision's annotation needs the class), then
    # the function defs.
    for tree, src in trees:
        _assign_pass(tree, ns, src)
    for tree, src in trees:
        for node in tree.body:
            if isinstance(node, ast.ClassDef) and node.name in CLASSES:
                exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec", flags=flag), ns)
    for tree, src in trees:
        _assign_pass(tree, ns, src)
    for tree, src in trees:
        for node in tree.body:
            if isinstance(node, ast.FunctionDef) and node.name in WANTED:
                exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec", flags=flag), ns)
    missing = (WANTED | set(CLASSES)) - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for name in CONSTS:
        if name not in ns:
            print("FATAL missing constant: %s" % name)
            sys.exit(2)
    return ns


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m


def _mod(name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    return m


def mat_decision(d):
    return NS_DEC(enabled=d["enabled"], source=d["source"], reason=d.get("reason", ""))


def proj_decision(d):
    return {"enabled": d.enabled, "source": d.source, "reason": d.reason}


def mat_config(d):
    if isinstance(d, dict):
        return types.SimpleNamespace(**d)
    return d


def _pg_service_label(self):
    return seam("pg_service_label", {"name": self.name})


def mat_pg(p):
    if not isinstance(p, dict):
        return p
    pg = types.SimpleNamespace(**p)
    pg.service_label = types.MethodType(_pg_service_label, pg)
    return pg


def mat_plan(d):
    if not isinstance(d, dict):
        return d
    kw = {}
    if "standalone_secondaries" in d:
        kw["standalone_secondaries"] = [mat_pg(p) for p in d["standalone_secondaries"]]
    if "blocked" in d:
        kw["blocked"] = d["blocked"]
    if "blockers" in d:
        kw["blockers"] = list(d["blockers"])
    return types.SimpleNamespace(**kw)


def install_stubs(ns):
    global NS_DEC
    NS_DEC = ns["MultiplexDecision"]

    real_sys = sys

    def _sys_exit(code):
        seam("sys_exit", {"code": code})
        raise SystemExit(code)

    ns["sys"] = types.SimpleNamespace(exit=_sys_exit, stderr=real_sys.stderr)
    ns["print"] = lambda *a, **k: seam("print", {"line": a[0] if a else None})

    def _log_debug(msg, *args, exc_info=False):
        seam("logger_debug", {"msg": msg, "exc_info": exc_info})

    def _log_info(msg, *args):
        seam("logger_info", {"msg": msg, "args": list(args)})

    def _log_warning(msg, *args, exc_info=False):
        d = {"msg": msg, "args": list(args)}
        if exc_info:
            d["exc_info"] = True
        seam("logger_warning", d)

    ns["logger"] = types.SimpleNamespace(debug=_log_debug, info=_log_info,
                                         warning=_log_warning)

    # ── external module stubs (in-function imports bind from sys.modules) ──
    hcli = _pkg("hermes_cli")

    cb = _mod("hermes_cli.config_backend")
    cb.config_exists = lambda path: seam("config_exists", {"path": posix(path)})
    hcli.config_backend = cb

    config = _mod("hermes_cli.config")
    config.read_user_config_raw = lambda path=None: seam(
        "read_user_config_raw", {"path": posix(path)})
    hcli.config = config

    profiles = _mod("hermes_cli.profiles")
    profiles.profile_is_standalone = lambda home: seam(
        "profile_is_standalone", {"home": posix(home)})

    def _profiles_to_serve(multiplex=False, include_parked=False, include_standalone=False):
        r = seam("profiles_to_serve",
                 {"multiplex": multiplex, "include_parked": include_parked})
        if r is None:
            return None
        return [(nm, PathDouble(h)) for nm, h in r]

    profiles.profiles_to_serve = _profiles_to_serve
    hcli.profiles = profiles

    served = _mod("hermes_cli.gateway_multiplex_served")
    served.recorded_served_profiles = lambda root=None: seam(
        "recorded_served_profiles", {"default_home": posix(root)})
    hcli.gateway_multiplex_served = served

    gm = _mod("hermes_cli.gateway_migrate")
    gm.MIGRATE_COMMAND = MIGRATE_COMMAND
    gm._host_supports_migration = lambda: seam("host_supports_migration", {})
    gm.build_migration_plan = lambda: mat_plan(seam("build_migration_plan", {}))
    gm._write_multiplex_flag = lambda home, value: seam(
        "write_multiplex_flag", {"default_home": posix(home), "value": value})
    hcli.gateway_migrate = gm

    hc = _mod("hermes_constants")

    def _default_root(**kw):
        # the real fn returns a Path; persist/consume do `/` on the result.
        # A scripted str models that Path; null stays None (truediv TypeError).
        r = seam("get_default_hermes_root", {})
        return PathDouble(r) if isinstance(r, str) else r

    hc.get_default_hermes_root = _default_root
    hc.get_hermes_home = lambda: seam("get_hermes_home", {})
    hc.profile_name_for_home = lambda home: seam(
        "profile_name_for_home", {"home": posix(home)})

    gateway = _pkg("gateway")
    gconfig = _mod("gateway.config")
    gconfig._bool_token = lambda value: seam("bool_token", {"value": value})
    gconfig._env_multiplex_profiles_override = lambda: seam("env_multiplex_override", {})
    gateway.config = gconfig

    gstatus = _mod("gateway.status")
    gstatus.owns_gateway_runtime_lock = lambda: seam("owns_gateway_runtime_lock", {})

    def _write_runtime_status(*, multiplex_standalone_reason=None, **kw):
        return seam("write_runtime_status",
                    {"multiplex_standalone_reason": multiplex_standalone_reason})

    gstatus.write_runtime_status = _write_runtime_status
    gstatus.read_runtime_status = lambda path=None: seam(
        "read_runtime_status", {"state_path": posix(path)})
    gstatus.runtime_status_pid_is_live = lambda record: seam(
        "runtime_status_pid_is_live", {"record": record})
    gateway.status = gstatus


# ── drive ──

def drive(ns, func, inp):
    """Returns (ret, state); ret is a raise spec on ANY BaseException."""
    name = PY[func]
    cfg = None
    CUR["stcfg"] = None
    CUR["raised"] = []
    if func in PENDING_FUNCS:
        ns["_pending_decision"] = [mat_decision(d) for d in (inp.get("pending") or [])]
    try:
        if func == "fail":
            ret = ns[name](*inp["lines"])
        elif func == "explicit_multiplex_flag":
            ret = ns[name](inp.get("default_home"))
        elif func == "settled_explicit_flag":
            ret = ns[name](inp.get("value"))
        elif func == "host_serves_named_profile":
            ret = ns[name](inp.get("default_home"), inp.get("home"))
        elif func == "default_gateway_multiplexes":
            ret = ns[name](inp.get("default_home"))
        elif func == "standalone_launcher":
            ret = ns[name]()
        elif func == "standalone_launcher_decision":
            cfg = mat_config(inp.get("config")); CUR["stcfg"] = cfg
            ret = ns[name](cfg)
        elif func == "implicit_multiplex_blocker":
            ret = ns[name]()
        elif func == "default_profile_home":
            ret = ns[name]()
        elif func == "persist_resolved_default":
            ret = ns[name](mat_decision(inp["decision"]), inp.get("default_home"))
        elif func == "retired_opt_out_notice_lines":
            ret = ns[name]()
        elif func == "consume_rewritten_notice":
            ret = ns[name](inp.get("default_home"))
        elif func == "box":
            ret = ns[name](inp.get("body"))
        elif func == "resolve_multiplex_mode":
            cfg = mat_config(inp.get("config")); CUR["stcfg"] = cfg
            ret = ns[name](cfg)
        elif func == "record_multiplex_decision":
            ret = ns[name](mat_decision(inp["decision"]))
        elif func == "publish_pending_multiplex_decision":
            ret = ns[name]()
        elif func == "log_multiplex_decision":
            ret = ns[name](mat_decision(inp["decision"]))
        elif func == "unserved_profiles":
            ret = ns[name]()
        elif func == "standalone_warning_lines":
            ret = ns[name](mat_decision(inp["decision"]), inp.get("unserved"))
        elif func == "recorded_standalone_reason":
            ret = ns[name](inp.get("state_path"))
        elif func == "recorded_standalone_warning_lines":
            ret = ns[name]()
        else:
            raise ValueError("unknown func: %s" % func)
        if func in DECISION_FUNCS and isinstance(ret, NS_DEC):
            ret = proj_decision(ret)
    except BaseException as e:
        spec = None
        for rr in CUR["raised"]:
            if e is rr.exc:
                spec = rr.spec
                break
        ret = spec if spec is not None else {"__raise__": type(e).__name__, "msg": str(e)}
    if func in CONFIG_FUNCS:
        state = None if cfg is None else {k: norm(v) for k, v in cfg.__dict__.items()}
    elif func in PENDING_FUNCS:
        state = {"pending": [proj_decision(d) for d in ns["_pending_decision"]]}
    else:
        state = {}
    return ret, state


def main():
    ns = extract()
    install_stubs(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        if func not in PY:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret, state = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"], "state": norm(state)}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE gateway_enroll_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_enroll_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


def selftest():
    """Drive every fixture case live; flag uniform AttributeError families that
    name a ported fn / seam stub (binding bugs) and report raise-spec coverage."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    ns = extract()
    install_stubs(ns)
    fixtures = os.path.join(os.path.dirname(__file__), "fixtures", "gateway_enroll_flow.jsonl")
    names = set(PY.values()) | WANTED
    bad = 0
    n = 0
    raises = {}
    per_func = {}
    for line in open(fixtures, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        rec = json.loads(line)
        func = rec["func"]
        inp = rec.get("in") or {}
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        n += 1
        per_func[func] = per_func.get(func, 0) + 1
        ret, state = drive(ns, func, inp)
        if isinstance(ret, dict) and "__raise__" in ret:
            kind = ret["__raise__"]
            raises[kind] = raises.get(kind, 0) + 1
            msg = ret.get("msg", "")
            if kind == "AttributeError" and any(("'%s'" % nm) in msg for nm in names):
                bad += 1
                print("SELFTEST binding-suspect %s: %s" % (func, msg))
    thin = [f for f in PY if per_func.get(f, 0) < 5]
    print("SELFTEST %d cases, %d binding-suspects, raises=%s" % (n, bad, raises))
    print("SELFTEST per-func counts: %s" % json.dumps(per_func, sort_keys=True))
    if thin:
        print("SELFTEST funcs with <5 cases: %s" % thin)
        bad += 1
    faulthandler.cancel_dump_traceback_later()
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        selftest()
    else:
        main()