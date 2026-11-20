#!/usr/bin/env python3
"""Oracle for gw_conn_ssh_flow2 — FLOW port of hermes_cli/gateway_connections_ssh_remote.py.

Drives the LIVE functions (AST-extracted from the quarry, no heavy import) with
scripted seams that mirror the C harness's universal seam. Compares the ordered
trace plus the projected return. Scripted {"__raise__": cls, "msg":...} raises a
real exception; BaseException-only classes pass every `except Exception`.
"""
import ast
import json
import os
import sys
import types
import re as _re
import hashlib
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "gwcss2_home"))
QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway_connections_ssh_remote.py")

FUNCS = [
 "_err","backend_profile_arg","mint_token","fingerprint_token","validate_ownership_id",
 "validate_spawn_nonce","ownership_directory","lockfile_path","connect_reservation_path",
 "spawn_log_path","spawn_token_path","shq","validate_remote_path","expand_remote_path",
 "assert_safe_remote_home","remote_install_root","probe_remote_platform",
 "probe_remote_hermes_home","assert_remote_install_update_clear","_is_executable",
 "locate_hermes","probe_hermes_version","remote_supports_ssh_ownership","_valid_int",
 "parse_lockfile","read_lockfile","_lock_json","write_lockfile","remove_lockfile",
 "exec_probe_verdict","remote_pid_alive","remote_process_creation_time","pid_is_our_serve",
 "_terminate","cleanup_stale","disconnect_remote","with_remote_update_mutex",
 "_spawn_payload_prefix","build_spawn_command","_remove_remote","spawn_remote_serve",
 "_now_iso","remote_profile_missing_error","scrape_ready_port","classify_ssh_reuse_proof",
 "lock_serves_connect","is_forward_bind_collision","open_forward",
 "wait_for_remote_spawn_completion","_skew_error","_try_reuse","_spawn_fresh",
 "_rollback_spawn","_probe_remote","connect",
]
PY = {n: n for n in FUNCS}
WANTED = set(FUNCS)
CUR = {"trace": [], "scripts": {}, "idx": {}, "deps": None}


def classify(c):
    """Byte-for-byte mirror of gcs2_classify in the C port."""
    c = c or ""
    if "uname -s" in c: return "uname"
    if "HERMES_HOME" in c: return "home"
    if c.startswith("python3 - ") and "hermes-update-in-progress" in c: return "marker_gate"
    if "[ -x " in c: return "is_exec"
    if "command -v hermes" in c: return "command_v"
    if "--version" in c: return "version"
    if "grep -q ssh-session-token-file" in c: return "help"
    if "fi; cat " in c: return "read_lock"
    if "umask 077 && mkdir -p" in c: return "write_lock"
    if "kill -9" in c: return "terminate9"
    if "kill -0 " in c and "ALIVE" in c: return "alive"
    if "reservation=" in c or "exit 75" in c: return "spawn"
    if "kill " in c and "i=$((i+1))" in c: return "terminate"
    if c.startswith("python3 -c"):
        if "expected_entries" in c: return "owned_check"
        if "fields[19]" in c or "/proc/" in c: return "creation_time"
        if "O_DIRECTORY" in c: return "token_upload"
        return "py_script"
    if c.startswith("cat "): return "cat_log"
    if "rm -f " in c: return "rm_file"
    return "other"


class SshReachError(Exception):
    def __init__(self, kind="unknown", message=""):
        super().__init__(message)
        self.kind = kind
        self.message = message

EXC_MAP = {
 "OSError":OSError,"FileNotFoundError":FileNotFoundError,"PermissionError":PermissionError,
 "FileExistsError":FileExistsError,"IsADirectoryError":IsADirectoryError,
 "NotADirectoryError":NotADirectoryError,"TimeoutError":TimeoutError,"InterruptedError":InterruptedError,
 "ConnectionError":ConnectionError,"ConnectionRefusedError":ConnectionRefusedError,
 "ValueError":ValueError,"UnicodeDecodeError":UnicodeDecodeError,"TypeError":TypeError,
 "KeyError":KeyError,"AttributeError":AttributeError,"RuntimeError":RuntimeError,
 "ImportError":ImportError,"ModuleNotFoundError":ModuleNotFoundError,"IndexError":IndexError,
 "LookupError":LookupError,"SyntaxError":SyntaxError,"StopIteration":StopIteration,
 "AssertionError":AssertionError,"Warning":Warning,"JSONDecodeError":__import__("json").JSONDecodeError,
 "SshReachError":SshReachError,"Exception":Exception,"BaseException":BaseException,
 "KeyboardInterrupt":KeyboardInterrupt,"SystemExit":SystemExit,"GeneratorExit":GeneratorExit,
 "NotImplementedError":NotImplementedError,"NameError":NameError,
}

def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        msg = r.get("msg", "")
        if name == "SshReachError":
            raise SshReachError(r.get("kind", "unknown"), msg)
        if name in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = cls.__new__(cls); Exception.__init__(e, msg); raise e
        if name == "SystemExit":
            raise SystemExit(msg)
        raise cls(msg)
    return r

def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0); CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


class SshDouble:
    """SshConnection stand-in: every exec/forward/cancel_forward is a seam."""
    def exec(self, command, stdin_data=None):
        return seam("exec", {"tag": classify(command)})
    def forward(self, local, remote):
        return seam("forward", {"local": local, "remote": remote})
    def cancel_forward(self, local, remote):
        return seam("cancel_forward", {"local": local, "remote": remote})

class DTDouble:
    @staticmethod
    def now(tz=None):
        r = seam("now_iso", {})
        return types.SimpleNamespace(isoformat=lambda timespec=None: r)


def extract():
    from dataclasses import dataclass, field
    import time as _time
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "hermes_cli.gateway_connections_ssh_remote_extracted",
        "json": json, "re": _re, "hashlib": hashlib, "os": os, "sys": sys,
        "dataclass": dataclass, "field": field,
        "Any": object, "Callable": object, "UTC": object(),
        "SshReachError": SshReachError,
        "SshConnection": SshDouble,
        "REMOTE_MARKER_GATE_PY": "marker_gate_script",
        "with_remote_timeout": lambda cmd: (seam("with_timeout", {}) or cmd),
        "logger": types.SimpleNamespace(debug=lambda *a, **k: None, info=lambda *a, **k: None,
                                        warning=lambda *a, **k: None, error=lambda *a, **k: None),
    }
    tree = ast.parse(src)
    lines = src.splitlines()
    # exec module constants (Assign + AnnAssign), restricted to simple Name targets
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            try: exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception: pass
        elif isinstance(node, ast.AnnAssign) and node.value is not None and isinstance(node.target, ast.Name):
            try: exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception: pass
    # exec dataclass definitions (with decorators kept)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in (
            "LockfileSkew", "SpawnPlan", "Spawned", "ConnectDeps", "Connected", "_Remote"
        ):
            first = node.decorator_list[0].lineno if node.decorator_list else node.lineno
            seg = "\n".join(lines[first - 1:node.end_lineno])
            try: exec(compile(seg, SRC, "exec"), ns)
            except Exception as ex:
                print("FATAL class %s: %s" % (node.name, ex)); sys.exit(2)
    # exec functions
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing)); sys.exit(2)
    return ns


def install_stubs(ns):
    import secrets as _secrets
    ns["time"] = types.SimpleNamespace(
        monotonic=lambda: seam("monotonic", {}),
        sleep=lambda s: seam("sleep", {"s": s}),
    )
    ns["secrets"] = types.SimpleNamespace(token_hex=lambda n: seam("token_hex", {"n": n}))
    ns["datetime"] = DTDouble
    # no-op i18n-ish or print channels live code may use
    ns["print"] = lambda *a, **k: None


def _mk_deps(ns, inp, ssh):
    def pick(): return seam("pick_port", {})
    def health(url): return seam("wait_health", {"url": url})
    def proof(url, token, nonce):
        return seam("probe_proof", {"url": url, "token": token, "nonce": nonce})
    return ns["ConnectDeps"](
        ssh=ssh, ownership_id=inp.get("ownership_id", ""), pick_local_port=pick,
        wait_for_health=health, probe_reuse_proof=proof,
        profile=inp.get("profile", ""), remote_hermes_path=inp.get("remote_hermes_path", ""),
        reuse_token=inp.get("reuse_token", ""),
        ready_timeout_s=inp.get("ready_timeout_s", 120.0))


def _mk_remote(ns, inp):
    return ns["_Remote"](
        platform=tuple(inp.get("platform", ["Linux", "x86_64"])),
        hermes_home=inp.get("hermes_home", "~/.hermes"),
        hermes_path=inp.get("hermes_path", "/usr/local/bin/hermes"),
        hermes_version=inp.get("hermes_version", ""),
        profile=inp.get("profile", ""))


def drive(ns, func, inp):
    f = ns[PY[func]]
    g = inp.get
    ssh = SshDouble()
    oid = g("ownership_id", "")
    if func == "_err": return f(g("kind", "unknown"), g("message", ""))
    if func == "backend_profile_arg": return f(g("profile"))
    if func == "mint_token": return f()
    if func == "fingerprint_token": return f(g("token", ""))
    if func == "validate_ownership_id": return f(g("ownership_id"))
    if func == "validate_spawn_nonce": return f(g("nonce"))
    if func == "ownership_directory": return f(oid)
    if func == "lockfile_path": return f(oid)
    if func == "connect_reservation_path": return f(oid)
    if func == "spawn_log_path": return f(oid, g("nonce"))
    if func == "spawn_token_path": return f(oid, g("nonce"))
    if func == "shq": return f(g("value", ""))
    if func == "validate_remote_path": return f(g("path", ""))
    if func == "expand_remote_path": return f(g("path", ""))
    if func == "assert_safe_remote_home": return f(g("home", ""))
    if func == "remote_install_root": return f(g("home", ""))
    if func == "probe_remote_platform": return f(ssh)
    if func == "probe_remote_hermes_home": return f(ssh)
    if func == "assert_remote_install_update_clear": return f(ssh, g("hermes_home", ""), g("hermes_path", ""))
    if func == "_is_executable": return f(ssh, g("candidate", ""))
    if func == "locate_hermes": return f(ssh, g("remote_hermes_path", ""))
    if func == "probe_hermes_version": return f(ssh, g("hermes_path", ""))
    if func == "remote_supports_ssh_ownership": return f(ssh, g("hermes_path", ""))
    if func == "_valid_int": return f(g("value"), g("lo", 0), g("hi", 0))
    if func == "parse_lockfile": return f(g("text", ""), oid)
    if func == "read_lockfile": return f(ssh, oid)
    if func == "_lock_json": return f(g("lock", {}))
    if func == "write_lockfile": return f(ssh, oid, g("lock", {}))
    if func == "remove_lockfile": return f(ssh, oid)
    if func == "exec_probe_verdict": return f(ssh, g("command", ""), tuple(g("sentinels", [])), g("failure", ""))
    if func == "remote_pid_alive": return f(ssh, g("pid"))
    if func == "remote_process_creation_time": return f(ssh, g("pid"))
    if func == "pid_is_our_serve": return f(ssh, g("lock", {}), oid)
    if func == "_terminate": return f(ssh, g("pid"))
    if func == "cleanup_stale":
        lk = g("lock")
        if isinstance(lk, dict) and lk.get("__skew__"):
            lk = ns["LockfileSkew"](reason=lk.get("reason", ""))
        return f(ssh, oid, lk, g("pid_alive", True))
    if func == "disconnect_remote": return f(ssh, oid)
    if func == "with_remote_update_mutex":
        r = f(g("command", ""), g("marker_word", ""), g("hermes_word", ""))
        return "mutex:%s:%d:%d" % (classify(g("command", "")),
                                  1 if g("marker_word") else 0, 1 if g("hermes_word") else 0)
    if func == "_spawn_payload_prefix":
        plan = ns["SpawnPlan"](**g("plan", {}))
        hermes, log, _det = f(plan)
        return [hermes, log, "detached_spawn"]
    if func == "build_spawn_command":
        plan = ns["SpawnPlan"](**g("plan", {}))
        f(plan)  # run real validation/raise path
        return "spawn_cmd"
    if func == "_remove_remote": return f(ssh, g("path", ""))
    if func == "spawn_remote_serve":
        def ac(): return seam("assert_clear", {})
        return f(ssh, hermes_path=g("hermes_path", ""), profile=g("profile", ""),
                 token=g("token", ""), ownership_id=oid, hermes_home=g("hermes_home", ""),
                 assert_install_clear=ac)
    if func == "_now_iso": return f()
    if func == "remote_profile_missing_error": return f(g("output", ""))
    if func == "scrape_ready_port": return f(ssh, g("log_path", ""), timeout_s=g("timeout_s", 120.0))
    if func == "classify_ssh_reuse_proof": return f(g("proof"), g("spawn_nonce", ""))
    if func == "lock_serves_connect":
        return f(g("lock", {}), profile=g("profile", ""), reuse_token=g("reuse_token", ""),
                 hermes_path=g("hermes_path", ""), hermes_home=g("hermes_home", ""))
    if func == "is_forward_bind_collision": return f(RuntimeError(g("error", "")))
    if func == "open_forward": return f(ssh, lambda: seam("pick_port", {}), g("remote_port", 0), g("attempts", 3))
    if func == "wait_for_remote_spawn_completion": return f(ssh, oid, g("timeout_s", 120.0))
    if func == "_skew_error":
        return f(oid, ns["LockfileSkew"](reason=g("skew", {}).get("reason", "")))
    if func == "_try_reuse":
        deps = _mk_deps(ns, g("deps", {}), ssh)
        remote = _mk_remote(ns, g("remote", {}))
        lock = json.loads(json.dumps(g("lock", {})))
        return f(deps, remote, lock)
    if func == "_spawn_fresh":
        deps = _mk_deps(ns, g("deps", {}), ssh)
        remote = _mk_remote(ns, g("remote", {}))
        return f(deps, remote)
    if func == "_rollback_spawn":
        spawned = ns["Spawned"](**g("spawned", {}))
        owned = g("owned", {})
        return f(ssh, oid, owned, spawned, g("local_port", 0), g("remote_port", 0), RuntimeError("err"))
    if func == "_probe_remote":
        deps = _mk_deps(ns, g("deps", {}), ssh)
        return f(deps)
    if func == "connect":
        deps = _mk_deps(ns, g("deps", {}), ssh)
        return f(deps)
    raise ValueError("unhandled func: " + func)


def norm(o):
    if isinstance(o, SshReachError):
        return {"__raise__": "SshReachError", "msg": str(o)}
    if isinstance(o, BaseException):
        return {"__raise__": type(o).__name__, "msg": str(o)}
    if hasattr(o, "__dataclass_fields__"):
        return {k: norm(v) for k, v in vars(o).items()}
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer(): return int(o)
    if isinstance(o, dict): return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list): return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(norm(o)), sort_keys=True, ensure_ascii=False, default=str))


def self_test(ns):
    import faulthandler
    faulthandler.dump_traceback_later(90, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "gw_conn_ssh_flow2.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").split("\n") if l.strip()]
    ff = {c["func"] for c in cases}
    if ff != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s" % (sorted(ff-set(PY)), sorted(set(PY)-ff)))
        sys.exit(2)
    install_stubs(ns)
    bad = 0
    for c in cases:
        inp = c["in"]
        CUR["trace"] = []; CUR["scripts"] = inp.get("seams", {}); CUR["idx"] = {}
        try:
            drive(ns, c["func"], inp)
        except BaseException as ex:
            spec = "%s: %s" % (type(ex).__name__, ex)
            if type(ex).__name__ in ("NameError", "AttributeError", "TypeError") and "unscripted seam" not in spec:
                print("SELFTEST ERR %s: %s" % (c["func"], spec)); bad += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELFTEST %d cases, %d errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


def main():
    if "--self-test" in sys.argv:
        self_test(extract()); return
    ns = extract(); install_stubs(ns)
    cases = 0; mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"): continue
        try: record = json.loads(line)
        except json.JSONDecodeError: continue
        func = record.get("func")
        if func not in PY:
            print("FATAL unknown func in harness output: %s" % func); sys.exit(2)
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["trace"] = []; CUR["scripts"] = inp.get("seams", {}); CUR["idx"] = {}
        try:
            ret = drive(ns, func, inp)
            want = {"ret": norm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            want = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)}, "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False)[:900])
            print("  PY: %s" % json.dumps(want, ensure_ascii=False)[:900])
    if cases == 0:
        print("ORACLE gw_conn_ssh_flow2: 0 cases (fail-closed)"); sys.exit(2)
    print("ORACLE gw_conn_ssh_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
