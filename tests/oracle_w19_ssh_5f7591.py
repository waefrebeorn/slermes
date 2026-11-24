#!/usr/bin/env python3
"""Differential oracle for tools/environments/ssh.py (gate w19_ssh_5f7591).

The live module builds a subprocess-driving backend whose import pulls the terminal
tool stack, so the oracle execs its module-level helpers and the SSHEnvironment
ClassDef against stub globals: Path is PurePosixPath for the socket-name helpers
(path shapes, not filesystem access) and an lstat fake for the ownership test.
"""
import ast, hashlib, json, pathlib, stat, sys, types, __future__
from pathlib import Path

for _s in (sys.stdin, sys.stdout):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
GATE = "w19_ssh_5f7591"
SRC = Path("C:/Users/eman5/hermes-agent/tools/environments/ssh.py")
_ANN = __future__.annotations.compiler_flag
WANT_FN = {"_sync_error", "_is_private_runtime_dir", "_ensure_ssh_available"}


class EnvConnectionError(RuntimeError):
    def __init__(self, reason, *, retry_hint=""):
        super().__init__(reason)
        self.reason = reason
        self.retry_hint = retry_hint


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "ssh_live", "stat": stat, "hashlib": hashlib, "os": types.SimpleNamespace(getuid=lambda: 0),
         "Path": pathlib.PurePosixPath, "_SSH_MULTIPLEX": True,
         "EnvironmentConnectionError": EnvConnectionError,
         "shutil": types.SimpleNamespace(which=lambda t: t),
         "Iterable": object, "BaseEnvironment": type("BaseEnvironment", (object,), {})}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANT_FN:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name == "SSHEnvironment":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
ENV = G["SSHEnvironment"]
ENV.__del__ = lambda self: None  # stub base has no __del__


def make_env(r):
    obj = ENV.__new__(ENV)
    obj.control_socket = pathlib.PurePosixPath(r.get("control_socket", "/tmp/x/a.sock"))
    obj.port = int(r.get("port", 22))
    obj.key_path = r.get("key_path", "")
    obj.user = r.get("user", "root")
    obj.host = r.get("host", "example")
    return obj


class _Stat:
    def __init__(self, mode, uid):
        self.st_mode = mode
        self.st_uid = uid


def run_case(r):
    f = r.get("func")
    if f == "sync_error":
        err = G["_sync_error"](r.get("reason", ""), r.get("subject", ""),
                               r.get("what", "the SSH connection"))
        return {"reason": err.reason, "retry_hint": err.retry_hint}
    if f == "private_runtime_dir":
        mode = int(r.get("mode", 0))
        uid = int(r.get("uid", -1))
        my_uid = int(r.get("my_uid", 0))
        fails = bool(r.get("lstat_fails"))

        class FakePath:
            def __init__(self, p):
                self.p = p

            def lstat(self):
                if fails:
                    raise OSError("nope")
                return _Stat(mode, uid)

        G["Path"] = FakePath
        G["os"] = types.SimpleNamespace(getuid=lambda: my_uid)
        try:
            return bool(G["_is_private_runtime_dir"](r.get("path", "/run/user/1000")))
        finally:
            G["Path"] = pathlib.PurePosixPath
    if f == "control_socket_for":
        obj = make_env(r)
        return str(ENV._control_socket_for(obj, tuple(r.get("send_env") or ())))
    if f == "target_flags":
        obj = make_env(r)
        return list(ENV._target_flags(obj, r.get("port_flag", "-p")))
    if f == "build_ssh_command":
        G["_SSH_MULTIPLEX"] = bool(r.get("multiplex", False))
        obj = make_env(r)
        return list(ENV._build_ssh_command(obj, r.get("extra_args"), tuple(r.get("send_env") or ())))
    if f == "ensure_ssh_available":
        available = set(r.get("available") or [])
        G["shutil"] = types.SimpleNamespace(which=lambda t: t if t in available else None)
        try:
            G["_ensure_ssh_available"]()
            return None
        except RuntimeError as exc:
            return str(exc)
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in {"sync_error", "private_runtime_dir", "control_socket_for", "target_flags",
                     "build_ssh_command", "ensure_ssh_available"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
