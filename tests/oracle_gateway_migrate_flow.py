#!/usr/bin/env python3
"""Oracle for port_gateway_migrate_flow.c — hermes_cli/gateway_migrate.py
migration FLOW cores + leaves (LIVE authority: the quarry gateway_migrate.py).

Drives the LIVE functions with SCRIPTED I/O: the targets (and the internal
helpers they compose) are AST-extracted from the quarry, the ProfileGateway /
MigrationPlan dataclasses are exec'd, module constants seeded (Assign AND
AnnAssign, plain-Name targets only), and every service-manager verb, config /
manifest write, file read/unlink, pid/owner probe, clock tick, foreign-module
call and print replaced by a recording stub returning the SAME scripted results
the C harness feeds its universal seam. Internal composition (the plan/profile
properties, _service_dict, _recorded_run_as_user, _manifest_path, _print,
_live_gateway_pid, _host_gateway_owner, _default_home, _write_manifest,
_wait_for_served, _wait_for_live_gateway via the REAL agent.deadline.poll_until,
_reconcile_standalone_runtime, and every ported leaf) runs REAL on both sides.
Compares the ordered DECISION SEQUENCE + return. Per-case BaseException catch
(SystemExit escapes contextlib.suppress(Exception) / except Exception and is
compared as a raise spec).

Input: stdin JSON lines from t_port_gateway_migrate_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE gateway_migrate_flow: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.

Fixture domain notes (kept identical on both sides):
- paths are posix strings; PathDouble normalizes like Windows Path.as_posix;
- service tuples are 2-element [kind, system] arrays (system bool);
- pids / uids use int literals; clock ticks use whole or .5 seconds;
- scripted raises never rely on KeyError str() (the live getpwnam path formats
  its own message); manifest text is strict JSON (json.loads == libjson).
"""
import ast
import builtins
import contextlib
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway_migrate.py")
DEADLINE_SRC = os.path.join(QUARRY, "agent", "deadline.py")

PY = {
    "service_label": "_service_label",
    "remove_verb": "_remove_verb",
    "service_from_dict": "_service_from_dict",
    "recorded_services": "_recorded_services",
    "manifest_secondaries": "_manifest_secondaries",
    "target_from_manifest": "_target_from_manifest",
    "manifest_not_yet_served": "_manifest_not_yet_served",
    "read_manifest": "_read_manifest",
    "own_gateway_pid": "_own_gateway_pid",
    "preflight_apply": "_preflight_apply",
    "restart_default": "_restart_default",
    "enable_default_at_boot": "_enable_default_at_boot",
    "remove_secondary_gateways": "_remove_secondary_gateways",
    "resume_target": "_resume_target",
    "resume_findings": "_resume_findings",
    "signalled_gateways": "_signalled_gateways",
    "apply_migration": "apply_migration",
    "rollback_migration": "rollback_migration",
}
# Internal helpers composed REAL by the flows (not separately driven).
INTERNAL = [
    "_service_dict", "_recorded_run_as_user", "_manifest_path", "_print",
    "_live_gateway_pid", "_host_gateway_owner", "_default_home", "_write_manifest",
    "_wait_for_served", "_wait_for_live_gateway", "_reconcile_standalone_runtime",
]
WANTED = set(PY.values()) | set(INTERNAL)
CLASSES = ["ProfileGateway", "MigrationPlan"]

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}


def _exc_class(name):
    cls = getattr(builtins, name, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls
    return type(name, (Exception,), {})


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise _exc_class(r["__raise__"])(r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
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
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


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
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    """Ordered, type-tagged canonical form: dict insertion order is semantic."""
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


# ── doubles ──

class PathDouble:
    """Path stand-in: join is pure; exists/read_text/unlink are scripted seams."""

    def __init__(self, p):
        if isinstance(p, PathDouble):
            s = p.s
        elif isinstance(p, str):
            s = p
        elif p is None:
            raise TypeError("expected str, bytes or os.PathLike object, not NoneType")
        else:
            raise TypeError("expected str, bytes or os.PathLike object, not %s"
                            % type(p).__name__)
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
        return seam("path_exists", {"path": self.s})

    def read_text(self, encoding=None):
        return seam("read_text", {"path": self.s, "encoding": encoding})

    def unlink(self, missing_ok=False):
        return seam("path_unlink", {"path": self.s, "missing_ok": missing_ok})

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


def owner_ns(script):
    if isinstance(script, dict):
        return types.SimpleNamespace(pid=script.get("pid"), home=script.get("home"))
    return script


def target_tuple(t):
    if t is None:
        return None
    return tuple(t)


def make_pg(d):
    return NS_PG(
        name=d["name"], home=PathDouble(d["home"]), pid=d.get("pid"),
        services=[tuple(s) for s in (d.get("services") or [])],
        run_as_user=d.get("run_as_user"), uid=d.get("uid"),
        runtime_home=PathDouble(d["runtime_home"]) if d.get("runtime_home") else None,
    )


def make_plan(d):
    return NS_PLAN(
        default_home=PathDouble(d["default_home"]),
        profiles=[make_pg(p) for p in (d.get("profiles") or [])],
        multiplex_flag_on=bool(d.get("multiplex_flag_on")),
        live_served=d.get("live_served"),
        manifest=d.get("manifest"),
        interrupted=bool(d.get("interrupted")),
        blockers=list(d.get("blockers") or []),
        notices=list(d.get("notices") or []),
        standalone_by_config=tuple(d.get("standalone_by_config") or []),
    )


# ── extraction ──

def extract():
    import logging
    from dataclasses import dataclass, field
    from typing import Callable, Iterator, Optional

    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("gateway_migrate_extracted")
    sys.modules["gateway_migrate_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "gateway_migrate_extracted",
        "json": json, "os": os, "sys": sys, "contextlib": contextlib,
        "logging": logging, "types": types,
        "Path": PathDouble, "dataclass": dataclass, "field": field,
        "Optional": Optional, "Callable": Callable, "Iterator": Iterator,
        "SimpleNamespace": types.SimpleNamespace,
    })
    tree = ast.parse(src)
    # Pass 1: module constants (Assign AND AnnAssign, plain-Name targets only).
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
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    import __future__
    flag = __future__.annotations.compiler_flag
    # Pass 2: dataclasses then the target/internal defs.
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flag), ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flag), ns)
    missing = (WANTED | set(CLASSES)) - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for name in ("MANIFEST_NAME", "MIGRATE_COMMAND", "_NOTHING_RECORDED",
                 "_COMPENSATOR_WAIT_SECONDS", "_SERVED_WAIT_SECONDS"):
        if name not in ns:
            print("FATAL missing constant: %s" % name)
            sys.exit(2)
    return ns


def extract_poll_until():
    """The REAL agent.deadline.poll_until, driven by the same seamed clock."""
    import __future__
    src = open(DEADLINE_SRC, encoding="utf-8").read()
    mod = types.ModuleType("agent_deadline_extracted")
    sys.modules["agent_deadline_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "agent_deadline_extracted",
        "time": types.SimpleNamespace(
            monotonic=lambda: seam("monotonic", {}),
            sleep=lambda sec: seam("sleep", {"seconds": sec}),
        ),
    })
    tree = ast.parse(src)
    found = False
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "poll_until":
            exec(compile(ast.Module(body=[node], type_ignores=[]), DEADLINE_SRC, "exec",
                         flags=__future__.annotations.compiler_flag), ns)
            found = True
    if not found:
        print("FATAL missing def: poll_until")
        sys.exit(2)
    return ns["poll_until"]


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


def install_stubs(ns):
    ns["print"] = lambda *a, **k: seam("print", {"line": a[0]} if a else {})
    ns["time"] = types.SimpleNamespace(
        monotonic=lambda: seam("monotonic", {}),
        sleep=lambda sec: seam("sleep", {"seconds": sec}),
        strftime=lambda fmt: seam("strftime", {"fmt": fmt}),
    )
    ns["_service_op"] = lambda kind, system, verb, home, *, run_as_user=None: seam(
        "service_op", {"kind": kind, "system": system, "verb": verb,
                       "home": posix(home), "run_as_user": run_as_user})
    ns["_stop_gateway_process"] = lambda home: seam("stop_gateway_process", {"home": posix(home)})
    ns["_spawn_detached_gateway"] = lambda home: seam("spawn_detached_gateway", {"home": posix(home)})
    ns["_write_multiplex_flag"] = lambda default_home, value: seam(
        "write_multiplex_flag", {"default_home": posix(default_home), "value": value})
    ns["_installed_services"] = lambda home: seam("installed_services", {"home": posix(home)})

    @contextlib.contextmanager
    def _home_env_stub(home):
        seam("home_env_enter", {"home": posix(home)})
        try:
            yield
        finally:
            seam("home_env_exit", {"home": posix(home)})

    ns["_home_env"] = _home_env_stub

    hcli = _pkg("hermes_cli")
    profiles = _mod("hermes_cli.profiles")
    profiles.profile_is_parked = lambda home: seam("profile_is_parked", {"home": posix(home)})
    hcli.profiles = profiles
    config = _mod("hermes_cli.config")
    config.require_readable_config_before_write = lambda path: seam(
        "require_readable_config", {"path": posix(path)})
    hcli.config = config
    gw = _mod("hermes_cli.gateway")
    gw._require_root_for_system_service = lambda ctx: seam("require_root", {"context": ctx})
    gw._system_service_identity = lambda: seam("system_service_identity", {})
    hcli.gateway = gw
    served = _mod("hermes_cli.gateway_multiplex_served")
    served.recorded_served_profiles = lambda home: seam(
        "recorded_served_profiles", {"default_home": posix(home)})
    hcli.gateway_multiplex_served = served

    gateway = _pkg("gateway")
    status = _mod("gateway.status")
    status.live_gateway_pid_for_home = lambda home: seam(
        "live_gateway_pid_for_home", {"home": posix(home)})
    status._same_hermes_home = lambda left, right: seam(
        "same_hermes_home", {"left": posix(left), "right": posix(right)})
    status.read_runtime_status = lambda path: seam("read_runtime_status", {"path": posix(path)})
    gateway.status = status
    host_attach = _mod("gateway.host_attach")
    host_attach.host_gateway = lambda: owner_ns(seam("host_gateway", {}))
    gateway.host_attach = host_attach

    hc = _pkg("hermes_constants")

    def _default_root():
        r = seam("get_default_hermes_root", {})
        return PathDouble(r) if r is not None else None

    hc.get_default_hermes_root = _default_root

    utils = _mod("utils")
    utils.atomic_json_write = lambda path, data, indent=None, separators=None: seam(
        "atomic_json_write", {"path": posix(path), "data": data, "indent": indent,
                              "separators": list(separators) if separators is not None else None})

    pwd = _mod("pwd")
    pwd.getpwnam = lambda name: seam("getpwnam", {"name": name})

    poll_until = extract_poll_until()
    ns["poll_until"] = poll_until
    agent = _pkg("agent")
    deadline = _mod("agent.deadline")
    deadline.poll_until = poll_until
    agent.deadline = deadline


# ── drive ──

def drive(ns, func, inp):
    """Returns ret; ret is a raise spec on ANY BaseException."""
    global NS_PG, NS_PLAN
    NS_PG = ns["ProfileGateway"]
    NS_PLAN = ns["MigrationPlan"]
    name = PY[func]
    try:
        if func in ("service_label", "remove_verb", "service_from_dict"):
            ret = ns[name](inp.get("service"))
        elif func == "recorded_services":
            ret = ns[name](inp.get("rec"))
        elif func in ("manifest_secondaries", "target_from_manifest"):
            ret = ns[name](inp.get("manifest"))
        elif func == "manifest_not_yet_served":
            ret = ns[name](inp.get("manifest"), inp.get("live_served"))
        elif func == "read_manifest":
            ret = ns[name](PathDouble(inp["default_home"]))
        elif func == "own_gateway_pid":
            ret = ns[name](PathDouble(inp["home"]), owner_ns(inp.get("owner")))
        elif func == "preflight_apply":
            ret = ns[name](make_plan(inp["plan"]), target_tuple(inp.get("target")),
                           inp.get("run_as_user"))
        elif func == "restart_default":
            ret = ns[name](make_pg(inp["plan_default"]), target_tuple(inp.get("target")),
                           PathDouble(inp["default_home"]), run_as_user=inp.get("run_as_user"))
        elif func in ("enable_default_at_boot", "remove_secondary_gateways",
                      "resume_target", "signalled_gateways"):
            ret = ns[name](make_plan(inp["plan"]))
        elif func == "resume_findings":
            ret = ns[name](make_plan(inp["plan"]), target_tuple(inp.get("target")),
                           inp.get("run_as_user"))
        elif func == "apply_migration":
            ret = ns[name](make_plan(inp["plan"]), served_wait=inp.get("served_wait", 90.0))
        elif func == "rollback_migration":
            h = inp.get("default_home")
            ret = ns[name](PathDouble(h) if h else None)
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


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
        ret = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE gateway_migrate_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_migrate_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
