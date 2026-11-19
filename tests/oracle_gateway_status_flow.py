#!/usr/bin/env python3
"""Oracle for port_gateway_status_flow.c — gateway/status.py liveness/status
FLOW cores + leaves (LIVE authority: the quarry gateway/status.py).

Drives the LIVE functions with SCRIPTED I/O: the 18 targets are AST-extracted
from the quarry (no heavy gateway import), module constants seeded (Assign AND
AnnAssign, plain-Name targets only), and every pid/lock/process probe, file
read, record builder, clock, foreign-module import and writer/owner/topology
method call replaced by a recording stub returning the SAME scripted results
the C harness feeds its universal seam. Internal composition (_start_times_
conflict inside _live_pid_from_record, _apply_set_fields/_coerce_session_store
inside _prepare_runtime_status_update, _prepare/_emit inside publish) runs REAL
on both sides. Compares the ordered DECISION SEQUENCE + return + mutated
module state, with ordered-dict comparison (insertion order is semantic for
the merge flows). Per-case BaseException catch (SystemExit escapes
contextlib.suppress(Exception) and is compared as a raise spec).

Input: stdin JSON lines from t_port_gateway_status_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "st":...}}
Summary: ORACLE gateway_status_flow: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.

Fixture domain notes (kept identical on both sides):
- platform keys / profile names / path scripts are strings or null;
- identity/record-builder scripts are objects or null;
- parked_profiles values and str()-coerced numbers use int literals or
  non-integral floats (parsed JSON loses int/float identity);
- heartbeat epochs/timestamps use whole or .5 seconds (exact microsecond
  arithmetic, no float-boundary drift);
- scripted raises never use KeyError (str(KeyError) reprs its argument; the
  live KeyError paths are exercised through missing record keys instead).
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
from datetime import datetime as _real_datetime, timezone, UTC
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "status.py")

PY = {
    "live_pid_from_record": "_live_pid_from_record",
    "read_live_pid_marker": "_read_live_pid_marker",
    "multiplexer_liveness": "multiplexer_liveness_for_profile",
    "live_gateway_pid_for_home": "live_gateway_pid_for_home",
    "prepare_runtime_status": "_prepare_runtime_status_update",
    "publish_runtime_status": "publish_runtime_status",
    "probe_lock_file": "_probe_lock_file",
    "terminate_verified_owner": "_terminate_verified_owner",
    "host_gateway_serves_home": "_host_gateway_serves_home",
    "merge_over_on_disk": "_merge_over_on_disk",
    "start_times_conflict": "_start_times_conflict",
    "pid_marker_names_self": "_pid_marker_names_self",
    "heartbeat_age_s": "runtime_status_heartbeat_age_s",
    "coerce_session_store": "_coerce_session_store",
    "profile_platforms": "profile_platforms_from_multiplexer",
    "profile_name_for_home": "profile_name_for_home",
    "apply_set_fields": "_apply_set_fields",
    "emit_transition": "_emit_runtime_status_transition",
}
WANTED = set(PY.values())

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
    # Deep-copy so live mutation of a result cannot corrupt the script/trace.
    r = json.loads(json.dumps(r))
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


def posix(p):
    if p is None:
        return None
    if isinstance(p, PathDouble):
        return p.as_posix()
    if isinstance(p, Path):
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
    """Path stand-in whose resolve() is a scripted seam (leaf drive only)."""

    def __init__(self, p):
        if isinstance(p, PathDouble):
            s = p.s
        elif p is None:
            raise TypeError("expected str, bytes or os.PathLike object, not NoneType")
        else:
            s = str(p)
        s = s.replace("\\", "/")
        while "//" in s:
            s = s.replace("//", "/")
        while len(s) > 1 and s.endswith("/"):
            s = s[:-1]
        self.s = s

    def resolve(self):
        return PathDouble(seam("path_resolve", {"path": self.s}))

    def as_posix(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __str__(self):
        return self.s

    def __eq__(self, other):
        if not isinstance(other, PathDouble):
            return NotImplemented
        return self.s.lower() == other.s.lower()

    def __hash__(self):
        return hash(self.s.lower())


class DateTimeDouble:
    @staticmethod
    def now(tz=None):
        epoch = seam("datetime_now", {})
        return _real_datetime.fromtimestamp(epoch, tz=tz or UTC)

    fromisoformat = staticmethod(_real_datetime.fromisoformat)


class WriterDouble:
    def submit(self, path, payload):
        return seam("writer.submit", {"path": posix(path), "payload": payload})


def _owner_double(script):
    if isinstance(script, dict):
        return types.SimpleNamespace(
            pid=script.get("pid"),
            serves=lambda name: seam("owner.serves", {"name": name}),
        )
    return script


def _topo_double(script):
    if isinstance(script, dict):
        home = script.get("home")
        return types.SimpleNamespace(
            pid=script.get("pid"),
            home=Path(home) if home else None,
            serves=lambda name: seam("topology.serves", {"name": name}),
        )
    return script


def _pathify(r):
    return Path(r) if isinstance(r, str) else r


# ── extraction ──

def extract():
    import contextlib
    import hashlib
    import logging
    import math
    import re
    import threading
    import time
    from dataclasses import dataclass, field
    from typing import Any, Callable, Iterable, NamedTuple, Optional

    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("gateway_status_extracted")
    sys.modules["gateway_status_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "gateway_status_extracted",
        "json": json, "os": os, "sys": sys, "re": re, "math": math,
        "time": time, "threading": threading, "contextlib": contextlib,
        "copy": copy, "hashlib": hashlib, "logging": logging,
        "datetime": _real_datetime, "timezone": timezone, "UTC": UTC,
        "Path": Path, "dataclass": dataclass, "field": field,
        "Any": Any, "Optional": Optional, "Callable": Callable,
        "Iterable": Iterable, "NamedTuple": NamedTuple,
    })
    tree = ast.parse(src)
    # Pass 1: module constants (Assign AND AnnAssign, plain-Name targets only;
    # a Subscript target like os.environ[...] would mutate the real env).
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
    # Pass 2: the target defs. future-annotations keeps annotations strings so
    # def-exec never evaluates them; default values still need the constants.
    import __future__
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=__future__.annotations.compiler_flag), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for name in ("_UNSET", "_runtime_status_state_lock"):
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


def install_stubs(ns):
    ns["_pid_from_record"] = lambda record, key="pid": seam(
        "pid_from_record", {"record": record, "key": key})
    ns["_pid_exists"] = lambda pid: seam("pid_exists", {"pid": pid})
    ns["_get_process_start_time"] = lambda pid: seam("get_process_start_time", {"pid": pid})
    ns["_read_json_file"] = lambda path, *, bare_pid_ok=False: seam(
        "read_json_file", {"path": posix(path)})
    ns["_marker_is_stale"] = lambda written_at, ttl_s: seam(
        "marker_is_stale", {"written_at": written_at, "ttl_s": ttl_s})
    ns["_update_pause_on_disk"] = lambda: seam("update_pause_on_disk", {})
    ns["_unlink_quietly"] = lambda path: seam("unlink_quietly", {"path": posix(path)})
    ns["get_running_pid_cached"] = lambda pid_path=None, *, cleanup_stale=True, ttl_seconds=1.0: seam(
        "get_running_pid_cached", {"pid_path": posix(pid_path), "cleanup_stale": cleanup_stale})
    ns["get_runtime_status_running_pid"] = lambda runtime=None, *, expected_home=None: seam(
        "get_runtime_status_running_pid",
        {"runtime": runtime, "expected_home": posix(expected_home)})
    ns["read_runtime_status"] = lambda path=None: seam(
        "read_runtime_status", {"path": posix(path)})
    ns["_build_pid_record"] = lambda: seam("build_pid_record", {})
    ns["_build_runtime_status_record"] = lambda: seam("build_runtime_status_record", {})
    ns["_utc_now_iso"] = lambda: seam("utc_now_iso", {})
    ns["_get_code_identity_fields"] = lambda: seam("get_code_identity_fields", {})
    ns["_get_runtime_status_path"] = lambda: Path(seam("get_runtime_status_path", {}))
    ns["parse_active_agents"] = lambda raw: seam("parse_active_agents", {"raw": raw})
    ns["_scoped_lock_owner_state"] = lambda owner_pid, owner_start_time: seam(
        "scoped_lock_owner_state", {"owner_pid": owner_pid, "owner_start_time": owner_start_time})
    ns["terminate_pid"] = lambda pid, *, force=False, expected_start_time=None: seam(
        "terminate_pid", {"pid": pid, "force": force, "expected_start_time": expected_start_time})
    ns["_wait_for_scoped_lock_owner_exit"] = lambda owner_pid, owner_start_time, *, attempts, delay: seam(
        "wait_for_scoped_lock_owner_exit",
        {"owner_pid": owner_pid, "owner_start_time": owner_start_time,
         "attempts": attempts, "delay": delay})
    ns["_try_acquire_file_lock"] = lambda handle: seam(
        "try_acquire_file_lock", {"handle": CUR["in"].get("handle", "h")})
    ns["_release_file_lock"] = lambda handle: seam(
        "release_file_lock", {"handle": CUR["in"].get("handle", "h")})
    ns["_same_hermes_home"] = lambda left, right: seam(
        "same_hermes_home", {"left": posix(left), "right": posix(right)})
    ns["shared_listener_mirror_platforms"] = lambda runtime, profile: seam(
        "shared_listener_mirror_platforms", {"runtime": runtime, "profile": profile})
    ns["normalize_updated_at"] = lambda value: seam("normalize_updated_at", {"value": value})
    ns["_profile_name_for_home"] = lambda home: seam(
        "profile_name_for_home_priv", {"profile_home": posix(home)})
    def _writer_stub():
        seam("get_runtime_status_writer", {})
        return WriterDouble()

    ns["_get_runtime_status_writer"] = _writer_stub
    ns["os"] = types.SimpleNamespace(getpid=lambda: seam("getpid", {}))
    ns["datetime"] = DateTimeDouble

    gateway = _pkg("gateway")
    host_attach = types.ModuleType("gateway.host_attach")
    host_attach.host_gateway = lambda: _owner_double(seam("host_gateway", {}))
    host_attach.profile_name_for_home = lambda ph: seam(
        "host_profile_name_for_home", {"profile_home": posix(ph)})
    sys.modules["gateway.host_attach"] = host_attach
    gateway.host_attach = host_attach
    host_topology = types.ModuleType("gateway.host_topology")
    host_topology.host_gateway_topology = lambda: _topo_double(seam("host_gateway_topology", {}))
    sys.modules["gateway.host_topology"] = host_topology
    gateway.host_topology = host_topology

    hcli = _pkg("hermes_cli")
    gw = types.ModuleType("hermes_cli.gateway")
    gw.named_profile_served_by_running_multiplexer = lambda name: seam(
        "named_profile_served_by_running_multiplexer", {"name": name})
    sys.modules["hermes_cli.gateway"] = gw
    hcli.gateway = gw
    served = types.ModuleType("hermes_cli.gateway_multiplex_served")
    served.live_default_gateway_pid = lambda: seam("live_default_gateway_pid", {})
    sys.modules["hermes_cli.gateway_multiplex_served"] = served
    hcli.gateway_multiplex_served = served

    hc = _pkg("hermes_constants")
    hc.get_default_hermes_root = lambda: _pathify(seam("get_default_hermes_root", {}))

    agent = _pkg("agent")
    monitoring = _pkg("agent.monitoring")
    health = types.ModuleType("agent.monitoring.gateway_health")
    health.emit_runtime_status_transition = lambda prev, payload: seam(
        "emit_runtime_status_transition", {"previous_payload": prev, "payload": payload})
    sys.modules["agent.monitoring.gateway_health"] = health
    monitoring.gateway_health = health
    agent.monitoring = monitoring


def make_coerce(ns, tag):
    if tag is None:
        return None
    if tag == "bool":
        return bool
    if tag == "parse_active_agents":
        return ns["parse_active_agents"]
    if tag == "active_work":
        return lambda v: list(v) if v else None
    if tag == "served_profiles":
        return lambda v: list(v or [])
    if tag == "parked_profiles":
        return lambda v: {str(k): str(r) for k, r in dict(v or {}).items()}
    if tag == "multiplex_reason":
        return lambda v: str(v) if v else None
    if tag == "session_store":
        return ns["_coerce_session_store"]
    raise ValueError("unknown coerce tag: %r" % tag)


# ── drive ──

def drive(ns, func, inp):
    """Returns (ret, st_out); ret is a raise spec on ANY BaseException."""
    st_out = None
    name = PY[func]
    try:
        if func in ("prepare_runtime_status", "publish_runtime_status"):
            st_in = inp.get("st") or {}
            sp = st_in.get("state_path")
            ns["_runtime_status_state_path"] = Path(sp) if sp else None
            ns["_runtime_status_state"] = copy.deepcopy(st_in.get("state"))
            try:
                ret = ns[name](**(inp.get("fields") or {}))
            finally:
                sp2 = ns["_runtime_status_state_path"]
                st_out = {"state_path": sp2.as_posix() if sp2 is not None else None,
                          "state": ns["_runtime_status_state"]}
        elif func == "apply_set_fields":
            target = copy.deepcopy(inp.get("target") or {})
            fields = []
            for f in inp.get("fields") or []:
                value = ns["_UNSET"] if "value" not in f else f["value"]
                fields.append((f["key"], value, make_coerce(ns, f.get("coerce"))))
            st_out = target
            ns[name](target, fields)
            ret = None
        elif func == "multiplexer_liveness":
            saved = ns["profile_name_for_home"]
            ns["profile_name_for_home"] = lambda profile_home: seam(
                "profile_name_for_home", {"profile_home": posix(profile_home)})
            try:
                ret = ns[name](Path(inp["profile_dir"]))
            finally:
                ns["profile_name_for_home"] = saved
        elif func == "profile_name_for_home":
            saved = ns["Path"]
            ns["Path"] = PathDouble
            try:
                ret = ns[name](inp["profile_home"])
            finally:
                ns["Path"] = saved
        elif func == "probe_lock_file":
            handle = types.SimpleNamespace(
                close=lambda: seam("handle.close", {"handle": inp.get("handle", "h")}))
            ret = ns[name](handle)
        elif func == "terminate_verified_owner":
            ret = ns[name](inp["owner_pid"], inp["owner_start_time"],
                           graceful_attempts=inp.get("graceful_attempts", 20),
                           force_attempts=inp.get("force_attempts", 20))
        elif func == "host_gateway_serves_home":
            ret = ns[name](inp["pid"], Path(inp["profile_home"]))
        elif func == "live_gateway_pid_for_home":
            ret = ns[name](Path(inp["home"]))
        elif func == "read_live_pid_marker":
            ret = ns[name](Path(inp["path"]), inp.get("ttl_s", 60))
        elif func == "merge_over_on_disk":
            ret = ns[name](Path(inp["path"]), inp.get("payload"))
        elif func == "live_pid_from_record":
            ret = ns[name](inp.get("record"))
        elif func == "start_times_conflict":
            ret = ns[name](inp.get("recorded"), inp.get("current"))
        elif func == "pid_marker_names_self":
            ret = ns[name](inp["target_pid"], inp.get("target_start_time"))
        elif func == "heartbeat_age_s":
            ret = ns[name](inp.get("record"))
        elif func == "coerce_session_store":
            ret = ns[name](inp.get("session_store"))
        elif func == "profile_platforms":
            ret = ns[name](inp.get("runtime"), inp.get("profile", "default"))
        elif func == "emit_transition":
            ret = ns[name](inp.get("previous"), inp.get("payload"))
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret, st_out


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
        ret, st_out = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if st_out is not None:
            want["st"] = norm(st_out)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE gateway_status_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_status_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
