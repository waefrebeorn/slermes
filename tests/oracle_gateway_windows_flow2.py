#!/usr/bin/env python3
"""Oracle for gateway_windows_flow2 — the UNCLAIMED remainder FLOW cores of
hermes_cli/gateway_windows.py (LIVE authority, py3.14 quarry venv).

Ports covered: start-attestation lifecycle (read/write/clear/horizon/check/
warning), scheduled-task XML drift (query/leaf-values/compare/drift/print/
registered launchers), launcher rendering + atomic write, and small report/
path helpers. Claimed siblings (_start_attestation_path, _attested_dead,
_exec_schtasks, _build_scheduled_task_xml, quoting/path helpers, ...) are
SEAMED; unclaimed siblings are extracted and run for real.

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the
quarry (no heavy import), module constants seeded (Assign AND AnnAssign),
every effect replaced by a recording stub returning the SAME scripted results
the C harness feeds its universal seam. Compares the ordered DECISION SEQUENCE
(trace of seam calls) plus the return value. Scripted {"__raise__": cls} raises
a REAL exception of that class; drive() catches BaseException so
KeyboardInterrupt/SystemExit pass through live except-Exception clauses exactly
as the C port propagates them. A MISSING seam key fail-closes (RuntimeError) on
both sides; a scripted null is a valid None result.

Input: stdin JSON lines from t_port_gateway_windows_flow2:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE gateway_windows_flow2: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
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
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "gw2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway_windows.py")

# C harness func -> live python name.
PY = {
    "attested_pids_from": "_attested_pids_from",
    "attestation_generation": "_attestation_generation",
    "attested_create_time": "_attested_create_time",
    "read_start_attestation": "_read_start_attestation",
    "clear_start_attestation": "_clear_start_attestation",
    "attestation_within_horizon": "_attestation_within_horizon",
    "write_start_attestation": "_write_start_attestation",
    "check_start_attestation": "check_start_attestation",
    "format_attestation_warning": "_format_attestation_warning",
    "task_run_hint": "_task_run_hint",
    "print_task_run_hint": "_print_task_run_hint",
    "print_start_attestation_warning": "_print_start_attestation_warning",
    "query_scheduled_task_xml": "_query_scheduled_task_xml",
    "task_xml_leaf_values": "_task_xml_leaf_values",
    "compare_scheduled_task_drift": "compare_scheduled_task_drift",
    "scheduled_task_drift": "scheduled_task_drift",
    "print_scheduled_task_drift": "_print_scheduled_task_drift",
    "registered_task_launchers": "registered_task_launchers",
    "launcher_owned_exit_line": "_launcher_owned_exit_line",
    "launcher_pythonpath_entries": "_launcher_pythonpath_entries",
    "render_launchers": "_render_launchers",
    "write_launcher_scripts": "_write_launcher_scripts",
    "atomic_write": "_atomic_write",
    "report_already_running": "_report_already_running",
    "startup_staging_path": "_startup_staging_path",
    "hermes_home": "_hermes_home",
    "mark_planned_restart": "_mark_planned_restart",
}
WANTED = set(PY.values())

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


EXC_MAP = {
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "FileExistsError": FileExistsError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "TimeoutError": TimeoutError, "ValueError": ValueError, "TypeError": TypeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "RuntimeError": RuntimeError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndexError": IndexError, "LookupError": LookupError, "SyntaxError": SyntaxError,
    "UnicodeDecodeError": UnicodeDecodeError, "Exception": Exception,
    "BaseException": BaseException, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit, "GeneratorExit": GeneratorExit,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


def posix(p):
    if p is None:
        return None
    if isinstance(p, PathDouble):
        return p.posix
    if isinstance(p, Path):
        return p.as_posix()
    return Path(p).as_posix()


class PathDouble:
    """A Path stand-in: POSIX stringification, pathlib-faithful suffix/name/
    parent algebra, and scripted exists/read/write/replace/unlink/mkdir."""

    def __init__(self, s):
        self.posix = Path(s).as_posix() if s is not None else ""

    def __str__(self):
        return self.posix

    def __fspath__(self):
        return self.posix

    def __truediv__(self, other):
        return PathDouble(self.posix + "/" + str(other))

    @property
    def name(self):
        p = self.posix.rstrip("/")
        if not p:
            return ""
        i = p.rfind("/")
        return p[i + 1:]

    @property
    def parent(self):
        p = self.posix
        if p == "/":
            return PathDouble("/")
        p = p.rstrip("/")
        i = p.rfind("/")
        if i < 0:
            return PathDouble(".")
        if i == 0:
            return PathDouble("/")
        return PathDouble(p[:i])

    def _join(self, name):
        par = str(self.parent)
        if par == ".":
            return PathDouble(name)
        if par == "/":
            return PathDouble("/" + name)
        return PathDouble(par + "/" + name)

    def with_suffix(self, suffix):
        name = self.name
        if name in ("", ".", ".."):
            raise ValueError("empty name")
        if suffix and not suffix.startswith("."):
            raise ValueError("Invalid suffix %r" % suffix)
        i = name.rfind(".")
        stem = name[:i] if i > 0 else name
        return self._join(stem + suffix)

    def with_name(self, name):
        if not name or "/" in name:
            raise ValueError("Invalid name %r" % name)
        return self._join(name)

    def exists(self):
        return seam("path_exists", {"path": self.posix})

    def mkdir(self, parents=False, exist_ok=False):
        return seam("path_mkdir", {"path": self.posix, "parents": parents, "exist_ok": exist_ok})

    def read_text(self, encoding=None):
        return seam("path_read_text", {"path": self.posix, "encoding": encoding})

    def write_text(self, data, encoding=None, errors=None, newline=None):
        return seam("path_write_text", {"path": self.posix, "content": data,
                                        "encoding": encoding, "errors": errors, "newline": newline})

    def replace(self, target):
        return seam("path_replace", {"src": self.posix, "dst": posix(target)})

    def unlink(self, missing_ok=False):
        return seam("path_unlink", {"path": self.posix, "missing_ok": missing_ok})


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PathDouble):
        return o.posix
    if isinstance(o, Path):
        return o.as_posix()
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


# ── extraction ──

def extract():
    import logging
    import re
    import time
    import uuid
    from datetime import datetime, timezone, UTC
    from xml.etree import ElementTree
    src = open(SRC, encoding="utf-8").read()

    class RecLogger:
        def debug(self, *a, **k): pass
        def info(self, *a, **k): pass
        def warning(self, *a, **k): pass
        def error(self, *a, **k): pass

    ns = {
        "__name__": "hermes_cli.gateway_windows_extracted",
        "json": json, "os": os, "re": re, "sys": sys, "time": time, "uuid": uuid,
        "Path": Path, "datetime": datetime, "timezone": timezone, "UTC": UTC,
        "ElementTree": ElementTree, "logging": logging,
        "logger": RecLogger(),
    }
    tree = ast.parse(src)
    const_nodes = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            const_nodes.append(node)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            const_nodes.append(node)
    for node in const_nodes:
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    # the module's `logger = logging.getLogger(__name__)` Assign overwrites the
    # seeded recorder — re-set it AFTER the constant loop.
    ns["logger"] = RecLogger()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    return ns


# ── recording stubs ──

def rec_print(*args, **kw):
    text = kw.get("sep", " ").join(str(a) for a in args)
    seam("print", {"text": text})


def path_stub(name):
    def stub():
        r = seam(name, {})
        return PathDouble(r) if r is not None else None
    return stub


def make_import_module(modname, attr, import_seam, fn):
    class M(types.ModuleType):
        def __getattr__(self, name):
            if name == attr:
                seam(import_seam, {})
                return fn
            raise AttributeError(name)
    m = M(modname)
    sys.modules[modname] = m
    return m


def install_stubs(ns):
    ns["print"] = rec_print

    def start_attestation_path(home=None):
        r = seam("start_attestation_path", {"home": posix(home)})
        return PathDouble(r) if r is not None else None
    ns["_start_attestation_path"] = start_attestation_path

    ns["is_task_registered"] = lambda: seam("is_task_registered", {})
    ns["get_task_name"] = lambda: seam("get_task_name", {})
    ns["get_task_script_path"] = path_stub("get_task_script_path")
    ns["get_startup_entry_path"] = path_stub("get_startup_entry_path")

    def build_scheduled_task_xml(task_name, launcher_path, user):
        return seam("build_scheduled_task_xml",
                    {"task_name": task_name, "script_path": posix(launcher_path), "user": user})
    ns["_build_scheduled_task_xml"] = build_scheduled_task_xml
    ns["_resolve_task_user"] = lambda: seam("resolve_task_user", {})

    def exec_schtasks(args, timeout=15):
        r = seam("exec_schtasks", {"argv": list(args), "timeout": timeout})
        return (r["code"], r["out"], r["err"])
    ns["_exec_schtasks"] = exec_schtasks

    ns["_quote_vbs_string"] = lambda value: seam("quote_vbs_string", {"value": value})
    ns["_preserve_hermes_home_path"] = lambda path: seam("preserve_hermes_home_path", {"value": posix(path)})
    ns["project_root_str"] = lambda: seam("project_root_str", {})

    def build_cmd(python_path, working_dir, hermes_home, profile_arg):
        return seam("build_gateway_cmd_script",
                    {"python_path": python_path, "working_dir": working_dir,
                     "hermes_home": hermes_home, "profile_arg": profile_arg})
    def build_vbs(python_path, working_dir, hermes_home, profile_arg):
        return seam("build_gateway_vbs_script",
                    {"python_path": python_path, "working_dir": working_dir,
                     "hermes_home": hermes_home, "profile_arg": profile_arg})
    ns["_build_gateway_cmd_script"] = build_cmd
    ns["_build_gateway_vbs_script"] = build_vbs

    ns["_attested_dead"] = lambda attested, current_pids, data=None, home=None: seam(
        "attested_dead", {"attested": attested, "current_pids": current_pids,
                          "data": data, "home": posix(home)})

    # time / datetime / uuid doubles (module-top bare globals)
    ns["time"] = types.SimpleNamespace(time=lambda: seam("time_time", {}))

    real_datetime = ns["datetime"]

    class DTDouble:
        fromisoformat = staticmethod(real_datetime.fromisoformat)

        @classmethod
        def now(cls, tz=None):
            r = seam("datetime_now", {})
            return types.SimpleNamespace(isoformat=lambda: r)
    ns["datetime"] = DTDouble
    ns["uuid"] = types.SimpleNamespace(uuid4=lambda: types.SimpleNamespace(hex=seam("uuid4_hex", {})))

    # in-function imports: module attribute access fires the import seam
    for pkg in ("hermes_cli", "gateway"):
        if pkg not in sys.modules:
            m = types.ModuleType(pkg)
            m.__path__ = []
            sys.modules[pkg] = m
    make_import_module("hermes_cli.config", "get_hermes_home",
                       "import_get_hermes_home", lambda: seam("get_hermes_home", {}))
    make_import_module("hermes_cli.process_identity", "_process_create_time",
                       "import_process_create_time", lambda pid: seam("process_create_time", {"pid": pid}))
    make_import_module("hermes_cli.gateway", "find_gateway_pids",
                       "import_find_gateway_pids", lambda *a, **k: seam("find_gateway_pids", {}))
    make_import_module("hermes_cli.gateway_service_owner", "service_definition_launchers",
                       "import_service_definition_launchers",
                       lambda definition: seam("service_definition_launchers", {"definition": definition}))
    make_import_module("hermes_cli.gateway_runtime_service_identity", "launcher_pid_file",
                       "import_launcher_pid_file", lambda hermes_home: seam("launcher_pid_file", {"hermes_home": hermes_home}))
    make_import_module("gateway.restart_notice", "write_planned_restart_marker",
                       "import_write_planned_restart_marker", lambda: seam("write_planned_restart_marker", {}))


# ── drive ──

def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    name = PY[func]
    f = ns[name]
    home = PathDouble(inp["home"]) if inp.get("home") is not None else None
    if func == "attested_pids_from":
        return f(inp.get("data"))
    if func == "attestation_generation":
        return f(inp.get("data"))
    if func == "attested_create_time":
        return f(inp.get("data"), inp.get("pid", 0))
    if func == "read_start_attestation":
        return f(home)
    if func == "clear_start_attestation":
        f(home)
        return None
    if func == "attestation_within_horizon":
        return f(inp.get("data"))
    if func == "write_start_attestation":
        f(inp.get("pids", []), inp.get("via", ""), home)
        return None
    if func == "check_start_attestation":
        return f(inp.get("current_pids"))
    if func == "format_attestation_warning":
        return f(inp.get("attested", []), inp.get("data"))
    if func == "task_run_hint":
        return f(inp.get("fmt", ""))
    if func == "print_task_run_hint":
        f(inp.get("fmt", ""))
        return None
    if func == "print_start_attestation_warning":
        f()
        return None
    if func == "query_scheduled_task_xml":
        return f(inp.get("task_name", ""))
    if func == "task_xml_leaf_values":
        return f(inp.get("xml", ""))
    if func == "compare_scheduled_task_drift":
        return f(inp.get("registered_xml", ""), inp.get("template_xml", ""))
    if func == "scheduled_task_drift":
        return f(inp.get("task_name", ""))
    if func == "print_scheduled_task_drift":
        f(inp.get("task_name", ""))
        return None
    if func == "registered_task_launchers":
        ns["sys"] = types.SimpleNamespace(platform=inp.get("platform", "win32"))
        return f()
    if func == "launcher_owned_exit_line":
        return f(inp.get("hermes_home", ""))
    if func == "launcher_pythonpath_entries":
        return f(inp.get("extra_pythonpath", []))
    if func == "render_launchers":
        sp = PathDouble(inp["script_path"]) if inp.get("script_path") is not None else None
        return f(tuple(inp.get("settings", [])), sp)
    if func == "write_launcher_scripts":
        scripts = [(PathDouble(p) if p is not None else None, c, e)
                   for p, c, e in inp.get("scripts", [])]
        f(scripts)
        return None
    if func == "atomic_write":
        f(PathDouble(inp["path"]), inp.get("content", ""), PathDouble(inp["tmp"]),
          encoding=inp.get("encoding", "utf-8"))
        return None
    if func == "report_already_running":
        f(inp.get("pids", []))
        return None
    if func == "startup_staging_path":
        return norm(f())
    if func == "hermes_home":
        return norm(f())
    if func == "mark_planned_restart":
        f()
        return None
    raise ValueError("unhandled func: %s" % func)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def self_test(ns):
    """Drive every fixture case live (no C): surface binding bugs / script gaps."""
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "gateway_windows_flow2.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").splitlines() if l.strip()]
    fixture_funcs = {c["func"] for c in cases}
    if fixture_funcs != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(fixture_funcs - set(PY)), sorted(set(PY) - fixture_funcs)))
        sys.exit(2)
    install_stubs(ns)
    bad = 0
    names = set(PY.values()) | set(PY)
    for c in cases:
        inp = c["in"]
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        try:
            drive(ns, c["func"], inp)
        except BaseException as ex:
            spec = "%s: %s" % (type(ex).__name__, ex)
            # an AttributeError naming a ported method or seam stub is a binding
            # bug, not a legitimate scripted raise
            if type(ex).__name__ == "AttributeError" and any(("'%s'" % n) in str(ex) for n in names):
                print("SELFTEST BINDING %s: %s" % (c["func"], spec))
                bad += 1
            elif type(ex).__name__ == "NameError":
                print("SELFTEST NAMEERROR %s: %s" % (c["func"], spec))
                bad += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


def main():
    if "--self-test" in sys.argv:
        self_test(extract())
        return
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
        try:
            ret = drive(ns, func, inp)
            want = {"ret": norm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            want = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                    "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE gateway_windows_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_windows_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()