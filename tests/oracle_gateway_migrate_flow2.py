#!/usr/bin/env python3
"""Oracle for gateway_migrate_flow2.c — hermes_cli/gateway_migrate.py UNCLAIMED
remainder (LIVE authority: the quarry gateway_migrate.py).

Drives the LIVE functions with SCRIPTED I/O: the 26 targets are AST-extracted
from the quarry, the ProfileGateway / MigrationPlan dataclasses are exec'd,
module constants seeded (Assign AND AnnAssign), and every service-manager verb,
config read/write, foreign-module call, print, the claimed flow1 siblings and
the claimed dataclass properties replaced by a recording stub returning the
SAME scripted results the C harness feeds its universal seam. Unclaimed
non-ported collaborators (_installed_services, _profile_gateway_config,
platform_serves_profile_prefix, the _home_env / _multiplex_read_mode context
managers) are likewise seamed. Internal composition (the ported targets calling
each other, the real dataclass field defaults) runs REAL on both sides.
Compares the ordered DECISION SEQUENCE + return + mutated plan state. Per-case
BaseException catch (SystemExit escapes except Exception / suppress(Exception)
and is compared as a raise spec).

Input: stdin JSON lines from t_port_gateway_migrate_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":...}}
Summary: ORACLE gateway_migrate_flow2: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.

Fixture domain notes (kept identical on both sides):
- paths are POSIX strings; PathDouble normalizes like Path.as_posix;
- service tuples are 2-element [kind, system] arrays (system bool);
- platform values are canonical lowercase Platform member values (the stub
  Enum has no _missing_ hook); cfg.platforms keys must be valid members;
- _credential_claims tuple keys project to "('pv', 'fp')" with simple-ASCII
  pv/fp (no quotes/backslashes/control chars) so Python repr == the C key;
- pc.extra is null/absent/dict; cfg.platforms present; pc.enabled present;
- _set_nested modelled for the fixed key "gateway.multiplex_profiles" with
  gateway absent or a dict; ports int or str; hosts str; pids/uids int literals.
- a MISSING seam script returns None on both sides (the harness script_call
  yields a NULL pointer); the oracle is fail-closed at the SUMMARY level
  (0 cases / missing defs -> exit 2) and every seam raise propagates.
"""
import ast
import builtins
import contextlib
import enum
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
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "gm2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway_migrate.py")

PY = {
    "profile_homes": "_profile_homes",
    "gateway_identity": "_gateway_identity",
    "windows_task_installed": "_windows_task_installed",
    "systemd_service_user": "_systemd_service_user",
    "s6_slot_op": "_s6_slot_op",
    "service_op": "_service_op",
    "stop_gateway_process": "_stop_gateway_process",
    "spawn_detached_gateway": "_spawn_detached_gateway",
    "read_multiplex_flag": "_read_multiplex_flag",
    "write_multiplex_flag": "_write_multiplex_flag",
    "credential_probe": "_credential_probe",
    "credential_claims": "_credential_claims",
    "credential_key_names": "_credential_key_names",
    "duplicate_credential_lines": "duplicate_credential_lines",
    "duplicate_credential_findings": "duplicate_credential_findings",
    "check_duplicate_credentials": "_check_duplicate_credentials",
    "listener_url": "_listener_url",
    "check_secondary_port_binders": "_check_secondary_port_binders",
    "load_profile_configs": "_load_profile_configs",
    "plan_tail": "_plan_tail",
    "format_update_warning": "format_update_warning",
    "host_supports_migration": "_host_supports_migration",
    "build_migration_plan": "build_migration_plan",
    "format_plan": "format_plan",
    "cmd_migrate": "cmd_migrate",
    "maybe_auto_migrate_after_update": "maybe_auto_migrate_after_update",
}
WANTED = set(PY.values())
CLASSES = ["ProfileGateway", "MigrationPlan"]
# Funcs that take a working plan and echo its mutated state.
PLAN_FUNCS = {
    "check_duplicate_credentials", "check_secondary_port_binders",
    "load_profile_configs", "plan_tail", "format_update_warning", "format_plan",
}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "stwin": None}
NS_PG = None
NS_PLAN = None


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
    # Snapshot ret at call time: the live caller may mutate the returned object
    # afterwards (e.g. _write_multiplex_flag pops/sets the config dict), and the
    # C harness records a json_copy snapshot — the trace must not alias it.
    CUR["trace"].append({"ev": name, "args": args, "ret": json.loads(json.dumps(r))})
    return maybe_raise(r)


# ── doubles ──

class PathDouble:
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

    def is_dir(self):
        return seam("path_is_dir", {"path": self.s})

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


class Platform(enum.Enum):
    LOCAL = "local"
    TELEGRAM = "telegram"
    DISCORD = "discord"
    WHATSAPP = "whatsapp"
    WHATSAPP_CLOUD = "whatsapp_cloud"
    SLACK = "slack"
    SIGNAL = "signal"
    MATTERMOST = "mattermost"
    MATRIX = "matrix"
    EMAIL = "email"
    SMS = "sms"
    DINGTALK = "dingtalk"
    API_SERVER = "api_server"
    WEBHOOK = "webhook"
    MSGRAPH_WEBHOOK = "msgraph_webhook"
    FEISHU = "feishu"
    WECOM = "wecom"
    WECOM_CALLBACK = "wecom_callback"
    WEIXIN = "weixin"
    BLUEBUBBLES = "bluebubbles"
    QQBOT = "qqbot"
    RELAY = "relay"


def posix(p):
    if p is None:
        return None
    if isinstance(p, (PathDouble, Path)):
        return p.as_posix()
    return str(p).replace("\\", "/")


def proj_owner(owner):
    if owner is None:
        return None
    return {"pid": owner.pid, "home": posix(owner.home)}


def owner_ns(script):
    if isinstance(script, dict):
        return types.SimpleNamespace(pid=script.get("pid"), home=script.get("home"))
    return script


def target_tuple(t):
    if t is None:
        return None
    return tuple(t)


def mat_pc(d):
    if isinstance(d, dict):
        return types.SimpleNamespace(**d)
    return d


def mat_cfg(d):
    if isinstance(d, dict):
        plats = {Platform(k): mat_pc(v) for k, v in (d.get("platforms") or {}).items()}
        return types.SimpleNamespace(platforms=plats)
    return d


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


def proj_pg(p):
    return {
        "name": p.name, "home": posix(p.home), "pid": p.pid,
        "services": [list(s) for s in p.services],
        "run_as_user": p.run_as_user, "uid": p.uid,
        "runtime_home": posix(p.runtime_home) if p.runtime_home is not None else None,
    }


def proj_plan(plan):
    return {
        "default_home": posix(plan.default_home),
        "profiles": [proj_pg(p) for p in plan.profiles],
        "multiplex_flag_on": plan.multiplex_flag_on,
        "live_served": plan.live_served,
        "manifest": plan.manifest,
        "interrupted": plan.interrupted,
        "blockers": list(plan.blockers),
        "notices": list(plan.notices),
        "standalone_by_config": list(plan.standalone_by_config),
    }


def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (Path, PathDouble)):
        return o.as_posix()
    if isinstance(o, Platform):
        return o.value
    if isinstance(o, BaseException):
        return str(o)
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in o.__dict__.items()}
    if isinstance(o, dict):
        return {(k.value if isinstance(k, Platform) else str(k)): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
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

def extract():
    import logging
    from dataclasses import dataclass, field
    from typing import Callable, Iterator, Optional

    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("gateway_migrate_flow2_extracted")
    sys.modules["gateway_migrate_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "gateway_migrate_flow2_extracted",
        "json": json, "os": os, "sys": sys, "contextlib": contextlib,
        "logging": logging, "types": types,
        "Path": PathDouble, "dataclass": dataclass, "field": field,
        "Optional": Optional, "Callable": Callable, "Iterator": Iterator,
        "SimpleNamespace": types.SimpleNamespace,
    })
    tree = ast.parse(src)
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
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flag), ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flag), ns)
    # _PREFLIGHT_CHECKS references the ported checks: re-exec AFTER the defs bind.
    for node in tree.body:
        if isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name) \
                and node.target.id == "_PREFLIGHT_CHECKS":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flag), ns)
    missing = (WANTED | set(CLASSES)) - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for name in ("MANIFEST_NAME", "MIGRATE_COMMAND", "_SERVED_WAIT_SECONDS", "_PREFLIGHT_CHECKS"):
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


def install_stubs(ns):
    global NS_PG, NS_PLAN
    NS_PG = ns["ProfileGateway"]
    NS_PLAN = ns["MigrationPlan"]

    real_sys = sys
    ns["sys"] = types.SimpleNamespace(
        exit=real_sys.exit, stderr=real_sys.stderr,
        stdin=types.SimpleNamespace(isatty=lambda: seam("stdin_isatty", {})),
    )
    ns["print"] = lambda *a, **k: seam(
        "print", {"line": a[0] if a else None,
                  "file": "stderr" if k.get("file") is real_sys.stderr else None})

    # ── claimed flow1 siblings (seamed, NOT re-ported) ──
    ns["_service_label"] = lambda service: seam("service_label", {"service": list(service)})
    ns["_remove_verb"] = lambda service: seam("remove_verb", {"service": list(service)})
    ns["_manifest_path"] = lambda default_home: PathDouble(
        seam("manifest_path", {"default_home": posix(default_home)}))
    ns["_print"] = lambda lines: seam("print_lines", {"lines": list(lines)})
    ns["_default_home"] = lambda: PathDouble(seam("default_home", {}))
    ns["_host_gateway_owner"] = lambda: owner_ns(seam("host_gateway_owner", {}))
    ns["_own_gateway_pid"] = lambda home, owner: seam(
        "own_gateway_pid", {"home": posix(home), "owner": proj_owner(owner)})
    ns["_installed_services"] = lambda home: [tuple(s) for s in (
        seam("installed_services", {"home": posix(home)}) or [])]
    ns["_read_manifest"] = lambda default_home: seam(
        "read_manifest", {"default_home": posix(default_home)})
    ns["_manifest_not_yet_served"] = lambda manifest, live_served: seam(
        "manifest_not_yet_served", {"manifest": manifest, "live_served": live_served})
    ns["_resume_target"] = lambda plan: tuple(seam("resume_target", {"plan": proj_plan(plan)}))
    ns["_signalled_gateways"] = lambda plan: seam("signalled_gateways", {"plan": proj_plan(plan)})
    ns["apply_migration"] = lambda plan, *, served_wait=90.0: seam(
        "apply_migration", {"plan": proj_plan(plan), "served_wait": served_wait})

    # ── unclaimed non-ported collaborators (seamed) ──
    ns["_profile_gateway_config"] = lambda home: mat_cfg(
        seam("profile_gateway_config", {"home": posix(home)}))
    ns["platform_serves_profile_prefix"] = lambda pv: seam(
        "serves_profile_prefix", {"platform_value": pv})

    @contextlib.contextmanager
    def _home_env_stub(home):
        seam("home_env_enter", {"home": posix(home)})
        try:
            yield
        finally:
            seam("home_env_exit", {"home": posix(home)})

    ns["_home_env"] = _home_env_stub

    @contextlib.contextmanager
    def _multiplex_read_mode_stub():
        seam("multiplex_read_enter", {})
        try:
            yield
        finally:
            seam("multiplex_read_exit", {})

    ns["_multiplex_read_mode"] = _multiplex_read_mode_stub

    # ── claimed dataclass properties / methods (seamed) ──
    NS_PLAN.secondaries = property(
        lambda self: [make_pg(x) for x in (seam("plan_secondaries", {}) or [])])
    NS_PLAN.standalone_secondaries = property(
        lambda self: [make_pg(x) for x in (seam("plan_standalone_secondaries", {}) or [])])
    NS_PLAN.default = property(lambda self: make_pg(seam("plan_default", {})))
    NS_PLAN.already_multiplexed = property(lambda self: seam("plan_already_multiplexed", {}))
    NS_PLAN.blocked = property(lambda self: seam("plan_blocked", {}))
    NS_PLAN.target_service_kind = lambda self: target_tuple(seam("plan_target_service_kind", {}))
    NS_PLAN.expected_served_names = property(
        lambda self: set(seam("plan_expected_served", {}) or []))
    NS_PG.service_label = lambda self: seam("pg_service_label", {"name": self.name})
    NS_PG.has_gateway = property(lambda self: seam("pg_has_gateway", {"name": self.name}))

    # ── external module stubs ──
    hcli = _pkg("hermes_cli")
    profiles = _mod("hermes_cli.profiles")
    profiles.profiles_to_serve = lambda multiplex=False, include_parked=False, include_standalone=False: [
        (nm, PathDouble(h)) for nm, h in seam(
            "profiles_to_serve", {"multiplex": multiplex, "include_parked": include_parked,
                                  "include_standalone": include_standalone})]
    profiles._stop_gateway_process = lambda home: seam("profiles_stop_gateway_process", {"home": posix(home)})
    hcli.profiles = profiles

    guards = _mod("hermes_cli.gateway_migrate_guards")
    guards.gateway_identity = lambda home, pid, services: tuple(seam(
        "gateway_identity", {"home": posix(home), "pid": pid, "services": [list(s) for s in services]}))
    guards.auto_migration_blockers = lambda plan: seam("auto_migration_blockers", {"plan": proj_plan(plan)})
    guards.auto_migration_opted_out = lambda home: seam("auto_migration_opted_out", {"default_home": posix(home)})
    guards.masked_default_unit = lambda home: seam("masked_default_unit", {"default_home": posix(home)})
    hcli.gateway_migrate_guards = guards

    gww = _mod("hermes_cli.gateway_windows")
    gww.is_task_registered = lambda: seam("is_task_registered", {})
    gww.is_startup_entry_installed = lambda: seam("is_startup_entry_installed", {})
    gww.install = lambda start_now=False, start_on_login=False: seam(
        "gww_install", {"start_now": start_now, "start_on_login": start_on_login})
    hcli.gateway_windows = gww

    gw = _mod("hermes_cli.gateway")
    gw._running_under_s6 = lambda: seam("running_under_s6", {})
    gw.get_systemd_unit_path = lambda system=False: PathDouble(seam("get_systemd_unit_path", {"system": system}))
    gw._read_systemd_user_from_unit = lambda path: seam("read_systemd_user_from_unit", {"path": posix(path)})
    gw.launchd_install = lambda: seam("launchd_install", {})
    gw.systemd_install = lambda system=False, run_as_user=None, non_interactive=False: seam(
        "systemd_install", {"system": system, "run_as_user": run_as_user, "non_interactive": non_interactive})

    def _run_systemctl(args, system=False, check=True, timeout=None):
        r = seam("run_systemctl", {"args": list(args), "system": system, "check": check, "timeout": timeout})
        return types.SimpleNamespace(**r) if isinstance(r, dict) else r

    gw._run_systemctl = _run_systemctl
    gw.get_service_name = lambda: seam("get_service_name", {})
    gw._service_call = lambda kind, verb, system: seam("service_call", {"kind": kind, "verb": verb, "system": system})
    gw._spawn_detached_gateway = lambda: seam("gw_spawn_detached", {})
    hcli.gateway = gw

    gmm = _mod("hermes_cli.gateway_multiplex_mode")
    gmm.explicit_multiplex_flag = lambda home: seam("explicit_multiplex_flag", {"default_home": posix(home)})
    hcli.gateway_multiplex_mode = gmm

    config = _mod("hermes_cli.config")
    config.require_readable_config_before_write = lambda path: seam("require_readable_config", {"path": posix(path)})

    def _set_nested(cfg, dotted_key, value):
        seam("set_nested", {"key": dotted_key, "value": value})
        parts = dotted_key.split(".")
        cur = cfg
        for p in parts[:-1]:
            nxt = cur.get(p)
            if not isinstance(nxt, dict):
                nxt = {}
                cur[p] = nxt
            cur = nxt
        cur[parts[-1]] = value

    config._set_nested = _set_nested
    config._write_user_config = lambda path, cfg: seam("write_user_config", {"path": posix(path), "config": cfg})
    hcli.config = config

    pc = _mod("hermes_cli.profile_channels")
    pc.credential_env_keys = lambda: seam("credential_env_keys", {})
    hcli.profile_channels = pc

    uu = _mod("hermes_cli.url_utils")
    uu.format_url_host = lambda host: seam("format_url_host", {"host": host})
    hcli.url_utils = uu

    s6 = _mod("hermes_cli.gateway_multiplex_s6")
    s6.bring_root_slot_up = lambda: seam("bring_root_slot_up", {})
    s6.park_named_slot = lambda name: seam("park_named_slot", {"name": name})
    s6.named_slot_name = lambda name: seam("named_slot_name", {"name": name})
    hcli.gateway_multiplex_s6 = s6

    sm = _mod("hermes_cli.service_manager")

    class S6ServiceManager:
        @property
        def scandir(self):
            return PathDouble(seam("s6_scandir", {}))

    sm.S6ServiceManager = S6ServiceManager
    hcli.service_manager = sm

    served = _mod("hermes_cli.gateway_multiplex_served")
    served.recorded_served_profiles = lambda home: seam("recorded_served_profiles", {"default_home": posix(home)})
    hcli.gateway_multiplex_served = served

    setup = _mod("hermes_cli.setup")
    setup.prompt_yes_no = lambda question, default=False: seam("prompt_yes_no", {"question": question, "default": default})
    hcli.setup = setup

    backends = _mod("hermes_cli.gateway_migrate_backends")
    backends.cmd_backends_report = lambda args: seam("cmd_backends_report", {"args": norm(args)})
    hcli.gateway_migrate_backends = backends

    hc = _pkg("hermes_constants")
    hc.profile_name_for_home = lambda home: seam("profile_name_for_home", {"home": posix(home)})

    gateway = _pkg("gateway")
    gconfig = _mod("gateway.config")
    gconfig.Platform = Platform
    gconfig.platform_binds_port = lambda pv, extra=None: seam("platform_binds_port", {"platform": pv, "extra": extra})
    gateway.config = gconfig

    grun = _mod("gateway.run")

    class GatewayRunner:
        @staticmethod
        def _adapter_credential_fingerprint(probe):
            return seam("adapter_credential_fingerprint", {"probe": norm(probe)})

    grun.GatewayRunner = GatewayRunner
    gateway.run = grun


# ── drive ──

def drive(ns, func, inp):
    """Returns (ret, state); ret is a raise spec on ANY BaseException."""
    name = PY[func]
    plan = None
    CUR["stwin"] = None
    try:
        if func == "profile_homes":
            ret = ns[name]()
        elif func == "gateway_identity":
            ret = ns[name](PathDouble(inp["home"]), inp.get("pid"),
                           [tuple(s) for s in (inp.get("services") or [])])
        elif func == "windows_task_installed":
            ret = ns[name]()
        elif func == "systemd_service_user":
            ret = ns[name](PathDouble(inp["home"]), [tuple(s) for s in (inp.get("services") or [])])
        elif func == "s6_slot_op":
            ret = ns[name](inp["verb"], PathDouble(inp["home"]))
        elif func == "service_op":
            ret = ns[name](inp["kind"], inp.get("system", False), inp["verb"],
                           PathDouble(inp["home"]), run_as_user=inp.get("run_as_user"))
        elif func == "stop_gateway_process":
            ret = ns[name](PathDouble(inp["home"]))
        elif func == "spawn_detached_gateway":
            ret = ns[name](PathDouble(inp["home"]))
        elif func == "read_multiplex_flag":
            ret = ns[name](PathDouble(inp["default_home"]))
        elif func == "write_multiplex_flag":
            ret = ns[name](PathDouble(inp["default_home"]), inp.get("value", False))
        elif func == "credential_probe":
            ret = ns[name](mat_pc(inp.get("platform_config")))
        elif func == "credential_claims":
            ret = ns[name](mat_cfg(inp.get("config")))
        elif func == "credential_key_names":
            ret = ns[name](inp["platform_value"])
        elif func == "duplicate_credential_lines":
            ret = ns[name]([(nm, mat_cfg(c)) for nm, c in inp["configs"]])
        elif func == "duplicate_credential_findings":
            ret = ns[name]()
        elif func == "check_duplicate_credentials":
            plan = make_plan(inp["plan"]); CUR["stwin"] = plan
            configs = {k: mat_cfg(v) for k, v in inp["configs"].items()}
            ret = ns[name](plan, configs)
        elif func == "listener_url":
            ret = ns[name](mat_cfg(inp.get("default_cfg")), inp["platform_value"], inp["profile"])
        elif func == "check_secondary_port_binders":
            plan = make_plan(inp["plan"]); CUR["stwin"] = plan
            configs = {k: mat_cfg(v) for k, v in inp["configs"].items()}
            ret = ns[name](plan, configs)
        elif func == "load_profile_configs":
            plan = make_plan(inp["plan"]); CUR["stwin"] = plan
            ret = ns[name](plan)
        elif func == "plan_tail":
            plan = make_plan(inp["plan"]); CUR["stwin"] = plan
            ret = ns[name](plan)
        elif func == "format_update_warning":
            plan = make_plan(inp["plan"]); CUR["stwin"] = plan
            ret = ns[name](plan, inp.get("auto_blockers") or [])
        elif func == "host_supports_migration":
            ret = ns[name]()
        elif func == "build_migration_plan":
            ret = proj_plan(ns[name]())
        elif func == "format_plan":
            plan = make_plan(inp["plan"]); CUR["stwin"] = plan
            ret = ns[name](plan, dry_run=inp.get("dry_run", False))
        elif func == "cmd_migrate":
            ret = ns[name](types.SimpleNamespace(**(inp.get("args") or {})))
        elif func == "maybe_auto_migrate_after_update":
            ret = ns[name]()
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__, "msg": str(e)}
    state = proj_plan(CUR["stwin"]) if CUR["stwin"] is not None else {}
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
        print("ORACLE gateway_migrate_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_migrate_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


def selftest():
    """Drive every fixture case live; flag uniform AttributeError families that
    name a ported fn / seam stub (binding bugs) and report raise-spec coverage."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    ns = extract()
    install_stubs(ns)
    fixtures = os.path.join(os.path.dirname(__file__), "fixtures", "gateway_migrate_flow2.jsonl")
    names = set(PY.values()) | set(WANTED)
    bad = 0
    n = 0
    raises = {}
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
        ret, state = drive(ns, func, inp)
        if isinstance(ret, dict) and "__raise__" in ret:
            kind = ret["__raise__"]
            raises[kind] = raises.get(kind, 0) + 1
            msg = ret.get("msg", "")
            if kind == "AttributeError" and any(("'%s'" % nm) in msg for nm in names):
                bad += 1
                print("SELFTEST binding-suspect %s: %s" % (func, msg))
    print("SELFTEST %d cases, %d binding-suspects, raises=%s" % (n, bad, raises))
    faulthandler.cancel_dump_traceback_later()
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        selftest()
    else:
        main()
