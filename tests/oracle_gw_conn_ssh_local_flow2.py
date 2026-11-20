#!/usr/bin/env python3
"""Oracle for gw_conn_ssh_local_flow2 — FLOW port of hermes_cli/gateway_connections_ssh.py.

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
import threading
from dataclasses import dataclass, field
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway_connections_ssh.py")

FUNCS = [
    "ownership_id", "_fingerprint", "_target", "_health_ok", "_probe_reuse_proof",
    "_wait_for_health", "_live_reuse", "_endpoint", "_teardown", "tunnel_status",
    "disconnect", "_bootstrap", "ensure_tunnel", "interactive_shell_argv", "close_all",
]
PY = {n: n for n in FUNCS}
WANTED = set(FUNCS)
CUR = {"trace": [], "scripts": {}, "idx": {}}


# ── exception classes (project-specific + full builtin hierarchy in C) ──
class SshReachError(Exception):
    def __init__(self, kind="unknown", message=""):
        super().__init__(message)
        self.kind = kind
        self.message = message


class HTTPError(Exception):
    pass


class TransportError(HTTPError):
    pass


class TimeoutException(TransportError):
    pass


class HTTPStatusError(HTTPError):
    pass


EXC_MAP = {
    "BaseException": BaseException, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit, "GeneratorExit": GeneratorExit,
    "Exception": Exception, "ArithmeticError": ArithmeticError, "BufferError": BufferError,
    "LookupError": LookupError, "AssertionError": AssertionError, "AttributeError": AttributeError,
    "EOFError": EOFError, "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "IndexError": IndexError, "KeyError": KeyError,
    "MemoryError": MemoryError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError, "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "TypeError": TypeError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "RuntimeWarning": RuntimeWarning,
    "SyntaxWarning": SyntaxWarning, "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning, "BytesWarning": BytesWarning,
    "ResourceWarning": ResourceWarning, "JSONDecodeError": json.JSONDecodeError,
    "SshReachError": SshReachError, "HTTPError": HTTPError, "TransportError": TransportError,
    "TimeoutException": TimeoutException, "HTTPStatusError": HTTPStatusError,
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


# ── doubles ──
class Response:
    def __init__(self, d):
        d = d or {}
        self.status_code = d.get("status", 0)
        self._body = d.get("body")

    def json(self):
        return self._body

    def raise_for_status(self):
        if self.status_code >= 400:
            raise HTTPStatusError("status %d" % self.status_code)


class SshDouble:
    def __init__(self, endpoint=None, ssh_binary="ssh"):
        self.endpoint = endpoint if endpoint is not None else {}
        self.ssh_binary = ssh_binary

    def open(self):
        return seam("ssh_open", {})

    def close(self):
        return seam("ssh_close", {})

    def is_alive(self):
        return seam("is_alive", {})

    def cancel_forward(self, local, remote):
        return seam("cancel_forward", {"local": local, "remote": remote})

    def forward(self, local, remote):
        return seam("forward", {"local": local, "remote": remote})

    def exec(self, command, stdin_data=None):
        return seam("exec", {"command": command})


class ConnectDeps:
    def __init__(self, **kw):
        self.__dict__.update(kw)


@dataclass
class SshEndpoint:
    host: str = ""
    user: str = ""
    port: int = 22
    key_path: str = ""
    control_path: str = ""


@dataclass
class ConnectedDouble:
    base_url: str = ""
    token: str = ""
    local_port: int = 0
    remote_port: int = 0
    pid: int = 0
    reused: bool = False
    spawn_nonce: str = ""
    hermes_version: str = ""


# ── extraction of live source ──
def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "hermes_cli.gateway_connections_ssh_extracted",
        "json": json, "re": _re, "hashlib": hashlib, "os": None, "sys": sys,
        "Path": Path, "threading": threading,
        "dataclass": dataclass, "field": field,
        "SshReachError": SshReachError, "SshConnection": SshDouble,
        "SshEndpoint": SshEndpoint, "Connected": ConnectedDouble,
        "ConnectDeps": ConnectDeps,
    }
    tree = ast.parse(src)
    lines = src.splitlines()
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            try: exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception: pass
        elif isinstance(node, ast.AnnAssign) and node.value is not None and isinstance(node.target, ast.Name):
            try: exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception: pass
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("SshTarget", "_Tunnel"):
            first = node.decorator_list[0].lineno if node.decorator_list else node.lineno
            seg = "\n".join(lines[first - 1:node.end_lineno])
            try: exec(compile(seg, SRC, "exec"), ns)
            except Exception as ex:
                print("FATAL class %s: %s" % (node.name, ex)); sys.exit(2)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef):
            try: exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception as ex:
                print("FATAL def %s: %s" % (node.name, ex)); sys.exit(2)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing)); sys.exit(2)
    return ns


def install_stubs(ns):
    ns["time"] = types.SimpleNamespace(
        monotonic=lambda: seam("monotonic", {}),
        sleep=lambda s: seam("sleep", {"s": s}),
    )
    ns["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(realpath=lambda p: p, abspath=lambda p: p),
    )
    ns["hashlib"] = hashlib
    ns["DEFAULT_READY_TIMEOUT_S"] = 120.0
    # store_root returns a Path; on Windows str(Path("/a/b")) becomes "\a\b".
    # The fixture domain supplies already-normalized POSIX roots, so identity.
    ns["store_root"] = lambda root=None: root if root is not None else "~/.hermes"
    httpx = types.SimpleNamespace(
        HTTPError=HTTPError, HTTPStatusError=HTTPStatusError,
        TransportError=TransportError, TimeoutException=TimeoutException,
        get=lambda url, **kw: seam("http_get", {"url": url, "token": kw.get("headers", {}).get("X-Hermes-Session-Token", "")}),
    )
    sys.modules["httpx"] = httpx
    sys.modules["hermes_constants"] = types.SimpleNamespace(get_hermes_home=lambda: "~/.hermes")
    ns["_http_get"] = lambda url, token="": Response(seam("http_get", {"url": url, "token": token}))
    ns["classify_ssh_reuse_proof"] = lambda proof, nonce: seam("classify_proof", {"proof": proof, "nonce": nonce})
    ns["connect"] = lambda deps: _connect_stub(deps)
    ns["disconnect_remote"] = lambda ssh, oid: seam("disconnect_remote", {"ownership_id": oid})
    ns["SshConnection"] = SshDouble
    ns["SshEndpoint"] = SshEndpoint
    ns["resolve_ssh_binary"] = lambda: seam("resolve_ssh_binary", {})
    ns["pick_local_port"] = lambda: 5000
    ns["default_control_dir"] = lambda root: seam("default_control_dir", {"root": str(root)})

    def csp(user, host, port, control_dir, *, key_path="", ownership_id=""):
        return seam("control_socket_path", {
            "user": user, "host": host, "port": port, "control_dir": str(control_dir),
            "key_path": key_path, "ownership_id": ownership_id,
        })
    ns["control_socket_path"] = csp
    ns["login_shell_quote"] = lambda value: seam("login_shell_quote", {"value": value})
    ns["build_exec_args"] = lambda endpoint, command: seam("build_exec_args", {
        "endpoint": endpoint, "command": command})


def _connect_stub(deps):
    r = seam("connect", {
        "ownership_id": deps.ownership_id,
        "profile": deps.profile,
        "remote_hermes_path": deps.remote_hermes_path,
        "reuse_token": deps.reuse_token,
        "ready_timeout_s": deps.ready_timeout_s,
    })
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def make_tunnel(d, cid=""):
    conn = types.SimpleNamespace(**(d.get("connected") or {}))
    ssh = SshDouble(d.get("endpoint") or {}, d.get("ssh_binary", "ssh"))
    return types.SimpleNamespace(
        ssh=ssh, ownership_id=d.get("ownership_id", ""), connected=conn,
        host=d.get("host", ""), fingerprint=tuple(d.get("fingerprint", [])),
    )


def drive(ns, func, inp):
    f = ns[PY[func]]
    g = inp.get
    connection = g("connection", {}) or {}
    tdict = g("tunnel")
    root = g("root")

    # Seed the module-global registry that the live stateful cores use.
    if func == "close_all":
        ns["_tunnels"] = {("c%d" % i): make_tunnel(t or {})
                          for i, t in enumerate(g("tunnels", []) or [])}
    else:
        tid = g("connection_id", connection.get("id", "")) or ""
        ns["_tunnels"] = {tid: make_tunnel(tdict)} if tdict is not None else {}

    if func == "ownership_id": return f(root, g("connection_id"))
    if func == "_fingerprint": return f(connection)
    if func == "_target":
        c = types.SimpleNamespace(**(g("connected", {}) or {}))
        return f(types.SimpleNamespace(connected=c))
    if func == "_health_ok": return f(g("base_url", ""))
    if func == "_probe_reuse_proof":
        return f(g("base_url", ""), g("token", ""), g("nonce", ""))
    if func == "_wait_for_health": return f(g("base_url", ""))
    if func == "_live_reuse": return f(make_tunnel(tdict or {}))
    if func == "_endpoint": return f(connection, root, g("ownership_id", ""))
    if func == "_teardown": return f(make_tunnel(tdict or {}))
    if func == "tunnel_status": return f(g("connection_id", ""))
    if func == "disconnect": return f(g("connection_id", ""))
    if func == "_bootstrap":
        prev = g("previous")
        return f(connection, root, make_tunnel(prev) if prev is not None else None)
    if func == "ensure_tunnel": return f(connection, root=root)
    if func == "interactive_shell_argv": return f(connection, g("cwd", ""), root=root)
    if func == "close_all": return f()
    raise ValueError("unhandled func: " + func)


def norm(o):
    if isinstance(o, BaseException):
        return {"__raise__": type(o).__name__, "msg": str(o)}
    if hasattr(o, "__dataclass_fields__") or (hasattr(o, "__dict__") and type(o).__module__ == "types"):
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
    faulthandler.dump_traceback_later(120, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "gw_conn_ssh_local_flow2.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").split("\n") if l.strip()]
    ff = {c["func"] for c in cases}
    if ff != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(ff - set(PY)), sorted(set(PY) - ff)))
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
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
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
            print("  C : %s" % json.dumps(got, ensure_ascii=False)[:1200])
            print("  PY: %s" % json.dumps(want, ensure_ascii=False)[:1200])
    if cases == 0:
        print("ORACLE gw_conn_ssh_local_flow2: 0 cases (fail-closed)"); sys.exit(2)
    print("ORACLE gw_conn_ssh_local_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
