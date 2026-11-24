#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/tcp_site.py (gate w20_tcpsite_d7fd64).

`has_live_listener`, `start_tcp_site` and `is_wildcard_host` are lifted out of
their modules' AST and executed as written (sync and async). The socket, the
aiohttp site and `sys.platform` are stubs the fixture scripts; every site the
live function builds, every start it attempts and every stop it makes is
recorded, so the retry policy is compared as a trace rather than as prose.
"""
import __future__
import ast
import asyncio
import errno
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "platforms" / "tcp_site.py"
INGRESS = QUARRY / "gateway" / "platforms" / "shared_ingress.py"
GATE = "w20_tcpsite_d7fd64"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

CASE = {}


class _Logger:
    def __init__(self):
        self.lines = []

    def info(self, fmt, *args):
        self.lines.append(fmt % args if args else fmt)

    def debug(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass


LOG = _Logger()


class _Site:
    def __init__(self, runner, host, port, reuse_address=None):
        self.host = host
        self.port = port
        self.reuse = reuse_address
        self.result = None
        CALLS.append(self)

    async def start(self):
        idx = len([c for c in CALLS if c.result is not None])
        script = [CASE.get("first") or "ok", CASE.get("second") or "ok"]
        want = script[idx] if idx < len(script) else "ok"
        if want != "ok":
            self.result = want
            code = getattr(errno, want)
            raise OSError(code, _ERRTEXT.get(want, want))
        self.result = "ok"
        return None

    async def stop(self):
        STOPS.append(self)
        return None


_ERRTEXT = {"EADDRINUSE": "Address already in use", "EACCES": "Permission denied"}
# Windows names the same condition WSAEADDRINUSE; the fixture speaks POSIX names.
_NAME_FIX = {"WSAEADDRINUSE": "EADDRINUSE", "WSAEACCES": "EACCES", "WSAETIMEDOUT": "ETIMEDOUT"}


def _errno_name(code):
    name = errno.errorcode.get(code, str(code))
    return _NAME_FIX.get(name, name)
CALLS = []
STOPS = []


class _Conn:
    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


def _create_connection(addr, timeout=None):
    probe = CASE.get("probe") or "refused"
    if probe == "ok":
        return _Conn()
    if probe == "refused":
        raise ConnectionRefusedError(errno.ECONNREFUSED, "refused")
    raise OSError(errno.ETIMEDOUT, "timed out")


socket_stub = types.ModuleType("socket")
socket_stub.create_connection = _create_connection
sys.modules.setdefault("socket_stub", socket_stub)

web_stub = types.ModuleType("aiohttp.web")
web_stub.TCPSite = _Site
aiohttp_stub = types.ModuleType("aiohttp")
aiohttp_stub.web = web_stub
sys.modules["aiohttp"] = aiohttp_stub
sys.modules["aiohttp.web"] = web_stub

sys_stub = types.SimpleNamespace(platform="linux")

G = {
    "__name__": "tcp_site_live",
    "logger": LOG,
    "socket": socket_stub,
    "errno": errno,
    "sys": sys_stub,
    "asyncio": asyncio,
}

IG = {"__name__": "shared_ingress_live"}


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)
        elif isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in names for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)


_exec(SRC, {"has_live_listener", "start_tcp_site"}, G)
_exec(INGRESS, {"is_wildcard_host", "_WILDCARD_HOSTS"}, IG)
G["is_wildcard_host"] = IG["is_wildcard_host"]


def expected(r):
    global CASE
    CASE = r
    CALLS.clear()
    STOPS.clear()
    LOG.lines.clear()
    sys_stub.platform = r.get("platform") or "linux"
    f = r.get("func")
    if f == "has_live_listener":
        return bool(G["has_live_listener"]("127.0.0.1", 1))
    if f == "is_wildcard_host":
        return bool(IG["is_wildcard_host"](r.get("host")))
    if f == "start":
        host = r.get("host")
        try:
            asyncio.run(G["start_tcp_site"](None, host, int(r.get("port") or 0),
                                            log_tag=r.get("log_tag") or ""))
        except OSError as exc:
            return {"status": "raised", "type": "OSError",
                    "errno": _errno_name(exc.errno)}
        return {"status": "ok", "value": {
            "calls": [{"reuse": c.reuse, "result": c.result} for c in CALLS],
            "stops": len(STOPS),
            "log": LOG.lines[-1] if LOG.lines else None,
            "outcome": "site",
        }}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"start", "has_live_listener", "is_wildcard_host"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
