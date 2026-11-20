#!/usr/bin/env python3
"""Oracle for gateway_cli_flow2.c — hermes_cli/gateway.py UNCLAIMED FLOW remainder
(LIVE authority: the quarry hermes_cli/gateway.py).

Drives the LIVE functions with SCRIPTED I/O: the 16 targets are AST-extracted
from the quarry (no heavy hermes_cli.gateway import), module constants seeded
(Assign AND AnnAssign, plain-Name targets only), and every unit-file read, env
read, foreign-module import, subprocess run, Path resolve/expanduser, pwd lookup
and print/log emission replaced by a recording stub returning the SAME scripted
results the C harness feeds its universal seam. Internal composition
(_installed_unit_ld_library_path inside _ld_library_path_line; user_systemd_unit_dir
+ _hermes_home_pinned_by_unit inside _bare_unit_pinned_home; _wait_for_target_user_bus
inside _ensure_system_service_linger) runs REAL on both sides. Claimed flow1 /
gateway_wrappers / gw_service_lifecycle siblings are SEAMED, never re-claimed.
Compares the ordered DECISION SEQUENCE + return value with ordered-dict
comparison (insertion order is semantic). Per-case BaseException catch
(SystemExit escapes except Exception and is compared as a raise spec; scripted
raises round-trip bare).

Input: stdin JSON lines from t_port_gateway_cli_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE gateway_cli_flow2: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.

FAIL-CLOSED script semantics (identical on both sides): a MISSING seams[name]
key returns a RuntimeError raise spec {"__raise__":"RuntimeError","msg":
"no script for seam: <name>"} (recorded as the seam ret, then propagated); a
scripted null is a valid None; an ARRAY is a per-call queue (last repeats); a
single value is a constant. {"__raise__": cls, "msg": ...} scripts an exception.

Fixture domain (kept identical on both sides): ASCII text inputs; POSIX lexical
paths (resolve/expanduser are SEAMS returning scripted strings, no real
filesystem); integral JSON numbers read as int; result/pwd doubles are JSON
objects carrying the attrs the live body reads (stderr/stdout/returncode;
pw_uid). Fixed live message constants carry byte-exact UTF-8.
"""
import ast
import builtins
import json
import os
import subprocess
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo is parent.parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "gc2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway.py")

PY = {
    "systemd_command": "_systemd_command",
    "completed_process_detail": "_completed_process_detail",
    "hermes_home_pinned_by_unit": "_hermes_home_pinned_by_unit",
    "installed_unit_ld_library_path": "_installed_unit_ld_library_path",
    "ld_library_path_line": "_ld_library_path_line",
    "loginctl_enable_linger": "_loginctl_enable_linger",
    "wait_for_target_user_bus": "_wait_for_target_user_bus",
    "user_systemd_unit_dir": "user_systemd_unit_dir",
    "user_runtime_dir": "_user_runtime_dir",
    "ensure_system_service_linger": "_ensure_system_service_linger",
    "refuses_container_user_scope_install": "refuses_container_user_scope_install",
    "is_apt_termux_install": "_is_apt_termux_install",
    "print_service_not_installed": "_print_service_not_installed",
    "print_runtime_health": "_print_runtime_health",
    "systemd_unit_belongs_to_current_home": "_systemd_unit_belongs_to_current_home",
    "bare_unit_pinned_home": "_bare_unit_pinned_home",
}
WANTED = set(PY.values())

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}


# ── exception construction ──

def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls == "CalledProcessError":
        return subprocess.CalledProcessError(spec.get("returncode", 1), spec.get("cmd", "systemctl"))
    if cls == "TimeoutExpired":
        return subprocess.TimeoutExpired(spec.get("cmd", "systemctl"), spec.get("timeout", 90))
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        if cls in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = b.__new__(b)
            Exception.__init__(e, msg)
            return e
        return b(msg)  # SystemExit/KeyboardInterrupt/GeneratorExit are the REAL builtins
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    if name not in CUR["scripts"]:
        # FAIL-CLOSED: missing script -> RuntimeError raise spec (recorded, then raised).
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r))  # deep copy: live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


def posix(p):
    if p is None:
        return None
    if isinstance(p, (PathDouble, Path)):
        return p.as_posix()
    return str(p).replace("\\", "/")


def _cp(r):
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


# ── normalization / canonical comparison ──

def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (Path, PathDouble)):
        return o.as_posix()
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, set):
        return sorted(norm(v) for v in o)
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items()}
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


# ── lexical POSIX path double (resolve/expanduser/exists are seams) ──

def _parent_lex(s):
    while len(s) > 1 and s.endswith("/"):
        s = s[:-1]
    idx = s.rfind("/")
    if idx < 0:
        return "."
    if idx == 0:
        return "/"
    return s[:idx]


def _name_lex(s):
    while len(s) > 1 and s.endswith("/"):
        s = s[:-1]
    idx = s.rfind("/")
    return s[idx + 1:] if idx >= 0 else s


def _join_lex(a, b):
    if not b:
        return a
    if b.startswith("/"):
        return b
    if not a:
        return b
    if a.endswith("/"):
        return a + b
    return a + "/" + b


class PathDouble:
    def __init__(self, p):
        if isinstance(p, PathDouble):
            s = p.s
        elif p is None:
            raise TypeError("argument should be a str or an os.PathLike object "
                            "where __fspath__ returns a str, not 'NoneType'")
        else:
            s = str(p)
        s = s.replace("\\", "/")
        while len(s) > 1 and s.endswith("/"):
            s = s[:-1]
        self.s = s

    def resolve(self):
        return PathDouble(seam("path_resolve", {"path": self.s}))

    def expanduser(self):
        return PathDouble(seam("path_expanduser", {"path": self.s}))

    def exists(self):
        return seam("path_exists", {"path": self.s})

    def is_dir(self):
        return seam("path_is_dir", {"path": self.s})

    @property
    def parent(self):
        return PathDouble(_parent_lex(self.s))

    @property
    def name(self):
        return _name_lex(self.s)

    def __truediv__(self, other):
        return PathDouble(_join_lex(self.s, str(other)))

    def as_posix(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __str__(self):
        return self.s

    def __eq__(self, other):
        if isinstance(other, PathDouble):
            return self.s == other.s
        if isinstance(other, Path):
            return self.s == other.as_posix()
        return NotImplemented

    def __hash__(self):
        return hash(self.s)


class RecLogger:
    def debug(self, msg, *a, **kw):
        args = {"msg": msg}
        if a:
            args["args"] = list(a)
        if kw.get("exc_info"):
            args["exc_info"] = True
        return seam("logger.debug", args)

    def warning(self, msg, *a, **kw):
        args = {"msg": msg}
        if a:
            args["args"] = list(a)
        return seam("logger.warning", args)


class EnvironDouble:
    def get(self, name, default=None):
        return seam("environ_get", {"name": name, "default": default})


class OsDouble:
    def __init__(self):
        self.environ = EnvironDouble()

    def getuid(self):
        return seam("getuid", {})

    def geteuid(self):
        return seam("geteuid", {})

    def getenv(self, name, default=None):
        return seam("os_getenv", {"name": name})


# ── extraction ──

def extract():
    import contextlib
    import logging
    import shutil
    import signal
    import socket
    import time
    import __future__

    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("gateway_cli_flow2_extracted")
    sys.modules["gateway_cli_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "gateway_cli_flow2_extracted",
        "json": json, "os": os, "sys": sys, "subprocess": subprocess,
        "contextlib": contextlib, "time": time, "socket": socket,
        "shutil": shutil, "signal": signal, "logging": logging, "Path": PathDouble,
    })
    tree = ast.parse(src)
    # Pass 1: module constants (plain-Name Assign/AnnAssign targets only).
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
    # Constants the Assign pass cannot bind (imported / computed) — seed explicitly.
    ns["PROJECT_ROOT"] = PathDouble("/proj/root")
    ns.setdefault("_SERVICE_BASE", "hermes-gateway")
    ns.setdefault("_SYSTEM_UNIT_DIR", PathDouble("/etc/systemd/system"))
    ns.setdefault("_CAPTURE_TEXT", dict(capture_output=True, text=True, encoding="utf-8", errors="replace"))
    # Pass 2: target defs (future-annotations keeps annotations as strings).
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=__future__.annotations.compiler_flag), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for name in ("_SERVICE_BASE", "_SYSTEM_UNIT_DIR", "_CAPTURE_TEXT", "PROJECT_ROOT"):
        if name not in ns:
            print("FATAL missing constant: %s" % name)
            sys.exit(2)
    ns["logger"] = RecLogger()  # re-set after the Assign pass (logger= getLogger Assign)
    return ns


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m


def _submod(pkg, name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    setattr(pkg, name.rsplit(".", 1)[-1], m)
    return m


def install_stubs(ns):
    # ── module-level names (claimed siblings + imported leaves) ──
    ns["get_hermes_home"] = lambda: PathDouble(seam("get_hermes_home", {}))
    ns["get_service_name"] = lambda: seam("get_service_name", {})
    ns["get_systemd_unit_path"] = lambda system=False: PathDouble(seam("get_systemd_unit_path", {"system": system}))
    ns["_unit_environment_value"] = lambda unit_path, name: seam("unit_environment_value", {"unit_path": posix(unit_path), "name": name})
    ns["_remap_path_for_user"] = lambda path, target_home_dir: seam("remap_path_for_user", {"path": path, "target_home_dir": target_home_dir})
    ns["_ensure_linger_enabled"] = lambda username=None, *, system=False: seam("ensure_linger_enabled", {"username": username, "system": system})
    ns["_systemd_unit_is_active"] = lambda system=False: seam("systemd_unit_is_active", {"system": system})
    ns["_systemd_cli_bits"] = lambda system: tuple(seam("systemd_cli_bits", {"system": system}))
    ns["_runtime_health_lines"] = lambda: seam("runtime_health_lines", {})
    ns["_print_info_lines"] = lambda *lines: seam("print_info_lines", {"lines": list(lines)})
    ns["_sync_hermes_home_from_systemd_unit"] = lambda system: seam("sync_hermes_home_from_systemd_unit", {"system": system})
    ns["_hermes_home_from_systemd_unit_file"] = lambda system=False: seam("hermes_home_from_systemd_unit_file", {"system": system})
    ns["_systemd_env_line"] = lambda name, value: seam("systemd_env_line", {"name": name, "value": value})
    ns["is_linux"] = lambda: seam("is_linux", {})
    ns["is_container"] = lambda: seam("is_container", {})
    ns["_path_exists_safe"] = lambda path: seam("path_exists_safe", {"path": posix(path)})
    # poll_until is a module-top import (agent.deadline) used as a BARE GLOBAL by
    # _wait_for_target_user_bus — seed it in the exec namespace, not only sys.modules.
    ns["poll_until"] = lambda fn, timeout, interval: seam("poll_until", {"timeout": timeout, "interval": interval})
    ns["print_warning"] = lambda text: seam("print_warning", {"text": text})
    ns["print_error"] = lambda text: seam("print_error", {"text": text})
    ns["print_info"] = lambda text: seam("print_info", {"text": text})

    def _print(*a):
        if len(a) == 1:
            return seam("print", {"text": a[0]})
        if not a:
            return seam("print", {})
        return seam("print", {"args": list(a)})
    ns["print"] = _print
    ns["logger"] = RecLogger()
    ns["os"] = OsDouble()
    ns["subprocess"] = types.SimpleNamespace(run=lambda args, **kw: seam("subprocess_run", {"args": args, **kw}))
    ns["gateway_service_owner"] = types.SimpleNamespace(
        foreign_systemd_unit_home=lambda s: seam("foreign_systemd_unit_home", {"system": s}))

    # ── in-function imports via sys.modules stubs ──
    hc = _pkg("hermes_constants")
    hc.get_real_home = lambda: seam("get_real_home", {})
    hc.is_container = lambda: seam("is_container", {})

    hcli = _pkg("hermes_cli")
    steward = _submod(hcli, "hermes_cli.steward")
    steward.STEWARD_APT_TERMUX = "apt-termux"
    steward.sealed_steward = lambda root: seam("sealed_steward", {"root": posix(root)})

    agent = _pkg("agent")
    deadline = _submod(agent, "agent.deadline")
    deadline.poll_until = lambda fn, timeout, interval: seam("poll_until", {"timeout": timeout, "interval": interval})

    pwd_mod = types.ModuleType("pwd")
    pwd_mod.getpwnam = lambda username: _cp(seam("pwd_getpwnam", {"username": username}))
    sys.modules["pwd"] = pwd_mod


# ── drive ──

def drive(ns, func, inp):
    name = PY[func]
    try:
        if func == "systemd_command":
            ret = ns[name](inp.get("argv"))
        elif func == "completed_process_detail":
            ret = ns[name](_cp(inp.get("result")))
        elif func == "hermes_home_pinned_by_unit":
            ret = ns[name](PathDouble(inp["unit_path"]))
        elif func == "installed_unit_ld_library_path":
            ret = ns[name](inp.get("system", False))
        elif func == "ld_library_path_line":
            ret = ns[name](inp.get("system", False), inp.get("target_home_dir"))
        elif func == "loginctl_enable_linger":
            ret = ns[name](inp["username"])
        elif func == "wait_for_target_user_bus":
            ret = ns[name](inp["uid"], inp.get("timeout", 5.0))
        elif func == "user_systemd_unit_dir":
            ret = ns[name]()
        elif func == "user_runtime_dir":
            ret = ns[name]()
        elif func == "ensure_system_service_linger":
            ret = ns[name](inp["username"])
        elif func == "refuses_container_user_scope_install":
            ret = ns[name](inp.get("system", False))
        elif func == "is_apt_termux_install":
            ret = ns[name]()
        elif func == "print_service_not_installed":
            ret = ns[name](inp.get("system", False))
        elif func == "print_runtime_health":
            ret = ns[name]()
        elif func == "systemd_unit_belongs_to_current_home":
            ret = ns[name](inp.get("system", False))
        elif func == "bare_unit_pinned_home":
            ret = ns[name]()
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec  # scripted raise round-trips bare
                break
        if ret is None:
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
        CUR["raised"] = []
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
        print("ORACLE gateway_cli_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_cli_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()