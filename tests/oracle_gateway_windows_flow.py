#!/usr/bin/env python3
"""Oracle for port_gateway_windows_flow.c — the Windows gateway service FLOW
cores of hermes_cli/gateway_windows.py (LIVE authority).

Drives the LIVE functions with SCRIPTED I/O: defs are AST-extracted from the
quarry (no heavy import), module constants seeded (Assign AND AnnAssign), and
every schtasks/filesystem/process/prompt/elevation/print/marker/sentinel/time
dependency replaced by a recording monkeypatch stub returning the SAME scripted
results the C harness feeds its universal seam. Compares the ordered DECISION
SEQUENCE (trace of seam calls) plus the return value.

Input: stdin JSON lines from t_port_gateway_windows_flow:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE gateway_windows_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
"""
import ast
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway_windows.py")

# C harness func -> live python name.
PY = {
    "start_or_report_running": "_start_or_report_running",
    "offer_elevated_install": "_offer_elevated_install",
    "reconcile_autostart_launchers": "reconcile_autostart_launchers",
    "redundant_autostart_entries": "redundant_autostart_entries",
    "remove_startup_entries": "_remove_startup_entries",
    "attested_death_generation": "attested_death_generation",
    "attested_dead": "_attested_dead",
    "attested_pid_exited_cleanly": "_attested_pid_exited_cleanly",
    "consume_start_attestation": "_consume_start_attestation",
    "reconcile_scheduled_task": "reconcile_scheduled_task",
    "live_gateway_pids": "_live_gateway_pids",
    "gateway_pid_identities": "_gateway_pid_identities",
}
WANTED = set(PY.values())
# Real internal helpers the targets call (extracted, run for real).
SIBLINGS = {
    "_attested_pids_from", "_attestation_generation", "_attested_create_time",
    "_attested_dead", "_attested_pid_exited_cleanly", "_remove_startup_entries",
}
ALL_DEFS = WANTED | SIBLINGS

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}


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
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


class GatewayClientError(Exception):
    pass


EXC_MAP = {
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "ValueError": ValueError,
    "ImportError": ImportError, "RuntimeError": RuntimeError,
    "GatewayClientError": GatewayClientError,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        cls = EXC_MAP.get(r["__raise__"], RuntimeError)
        raise cls(r.get("msg", ""))
    return r


def posix(p):
    if p is None:
        return None
    if isinstance(p, PathDouble):
        return p.posix
    return Path(p).as_posix()


class PathDouble:
    """A Path stand-in that stringifies POSIX and scripts exists/unlink/read."""
    def __init__(self, s):
        self.posix = Path(s).as_posix() if s is not None else ""

    def __str__(self):
        return self.posix

    def __fspath__(self):
        return self.posix

    def __truediv__(self, other):
        return PathDouble(self.posix + "/" + str(other))

    def exists(self):
        return seam("path_exists", {"path": self.posix})

    def unlink(self, missing_ok=False):
        return seam("path_unlink", {"path": self.posix})

    def read_text(self, encoding=None):
        return seam("read_text", {"path": self.posix, "encoding": encoding})


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
    from datetime import datetime, timezone, UTC
    src = open(SRC, encoding="utf-8").read()

    class RecLogger:
        def debug(self, *a, **k): pass
        def info(self, *a, **k): pass
        def warning(self, *a, **k): pass
        def error(self, *a, **k): pass

    ns = {
        "__name__": "hermes_cli.gateway_windows_extracted",
        "json": json, "os": os, "re": re, "sys": sys, "time": time,
        "Path": Path, "datetime": datetime, "timezone": timezone, "UTC": UTC,
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
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in ALL_DEFS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    missing = ALL_DEFS - set(ns)
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


def install_stubs(ns):
    ns["print"] = rec_print
    ns["is_task_registered"] = lambda: seam("is_task_registered", {})
    ns["get_task_name"] = lambda: seam("get_task_name", {})
    ns["get_startup_entry_path"] = path_stub("get_startup_entry_path")
    ns["_legacy_startup_entry_path"] = path_stub("legacy_startup_entry_path")
    ns["_startup_staging_path"] = path_stub("startup_staging_path")
    ns["get_task_script_path"] = path_stub("get_task_script_path")
    ns["_hermes_home"] = lambda: PathDouble(seam("hermes_home", {}))

    def install_startup_entry(script_path):
        r = seam("install_startup_entry", {"script_path": posix(script_path)})
        return PathDouble(r) if r is not None else None
    ns["_install_startup_entry"] = install_startup_entry
    ns["_write_task_script"] = lambda: PathDouble(seam("write_task_script", {}))

    def prompt_yes_no(question, default=False):
        return seam("prompt_yes_no", {"question": question, "default": default})
    ns_prompt = types.ModuleType("hermes_cli.setup")
    ns_prompt.prompt_yes_no = prompt_yes_no
    sys.modules["hermes_cli.setup"] = ns_prompt

    def launch_elevated_install(force=False, *, start_now=None, start_on_login=None):
        return seam("launch_elevated_install",
                    {"force": force, "start_now": start_now, "start_on_login": start_on_login})
    ns["_launch_elevated_install"] = launch_elevated_install

    ns["_gateway_pids"] = lambda: seam("gateway_pids", {})
    ns["_spawn_detached"] = lambda *a, **k: seam("spawn_detached", {})
    ns["_report_gateway_start"] = lambda via: seam("report_gateway_start", {"via": via})
    ns["_report_already_running"] = lambda pids: seam("report_already_running", {"pids": pids})

    ns["_read_start_attestation"] = lambda home=None: seam("read_start_attestation", {"home": posix(home)})
    ns["_clear_start_attestation"] = lambda home=None: seam("clear_start_attestation", {"home": posix(home)})
    ns["_attestation_within_horizon"] = lambda data: seam("within_horizon", {"data": data})

    ns["scheduled_task_drift"] = lambda task_name: seam("scheduled_task_drift", {"task_name": task_name})

    def install_scheduled_task(task_name, script_path):
        r = seam("install_scheduled_task", {"task_name": task_name, "script_path": posix(script_path)})
        return (r["ok"], r["detail"])
    ns["_install_scheduled_task"] = install_scheduled_task

    # gateway.lifecycle_ledger.get_lifecycle_sentinel_path
    ledger = types.ModuleType("gateway.lifecycle_ledger")
    ledger.get_lifecycle_sentinel_path = lambda home: PathDouble(seam("get_lifecycle_sentinel_path", {"home": posix(home)}))
    gw = sys.modules.get("gateway") or types.ModuleType("gateway")
    gw.__path__ = []
    sys.modules["gateway"] = gw
    sys.modules["gateway.lifecycle_ledger"] = ledger

    # gateway.status: get_running_pid + get_process_start_time (import seam)
    def rec_get_running_pid(path=None, cleanup_stale=True):
        return seam("get_running_pid", {"path": posix(path), "cleanup_stale": cleanup_stale})

    def rec_get_process_start_time(pid):
        return seam("get_process_start_time", {"pid": pid})

    class StatusModule(types.ModuleType):
        def __getattr__(self, name):
            if name == "get_running_pid":
                return rec_get_running_pid
            if name == "get_process_start_time":
                r = seam("import_start_time", {})
                # maybe_raise already ran in seam; if ImportError it raised.
                return rec_get_process_start_time
            raise AttributeError(name)
    status = StatusModule("gateway.status")
    sys.modules["gateway.status"] = status

    # hermes_cli.gateway.find_gateway_pids
    hcli = sys.modules.get("hermes_cli") or types.ModuleType("hermes_cli")
    hcli.__path__ = []
    sys.modules["hermes_cli"] = hcli
    gwmod = types.ModuleType("hermes_cli.gateway")
    gwmod.find_gateway_pids = lambda all_profiles=False: seam("find_gateway_pids", {"all_profiles": all_profiles})
    sys.modules["hermes_cli.gateway"] = gwmod


# ── drive ──

def drive(ns, func, inp):
    name = PY[func]
    f = ns[name]
    home = PathDouble(inp["home"]) if inp.get("home") is not None else None
    if func == "start_or_report_running":
        rp = inp.get("running_pids")
        f(rp)
        return None
    if func == "offer_elevated_install":
        return f(inp.get("headline", ""), inp.get("force", False),
                 inp.get("start_now", False), inp.get("start_on_login", False))
    if func == "reconcile_autostart_launchers":
        done, warnings = f()
        return {"done": done, "warnings": warnings}
    if func == "redundant_autostart_entries":
        return [norm(p) for p in f()]
    if func == "remove_startup_entries":
        done, warnings = f()
        return {"done": done, "warnings": warnings}
    if func == "attested_death_generation":
        return f(inp.get("current_pids", []), home)
    if func == "attested_dead":
        return f(inp.get("attested", []), inp.get("current_pids", []), inp.get("data"), home)
    if func == "attested_pid_exited_cleanly":
        return f(inp.get("pid", 0), inp.get("create_time"), home)
    if func == "consume_start_attestation":
        f(inp.get("generation", ""), home)
        return None
    if func == "reconcile_scheduled_task":
        return f(inp.get("task_name", ""))
    if func == "live_gateway_pids":
        pf = None
        if inp.get("pid_filter_present"):
            pf = lambda pids: seam("pid_filter", {"pids": pids})
        return f(inp.get("all_profiles", False), home, pf)
    if func == "gateway_pid_identities":
        return norm(f(inp.get("pids", [])))
    raise ValueError("unknown func: %s" % func)


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
        try:
            ret = drive(ns, func, inp)
        except Exception as ex:
            mism += 1
            print("PYERR %s: %r" % (func, ex))
            continue
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE gateway_windows_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_windows_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()