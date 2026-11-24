#!/usr/bin/env python3
"""Differential oracle for hermes_cli/shared_session_attach.py (gate w20_sharedsess_a7d92d).

The live functions `_local_origin`, `discover_attach_url` and
`configure_tui_attachment` are lifted out of the module's AST (sync and async)
and executed for real. Everything they touch that is not pure is a stub the
fixture scripts: the registry snapshot, `session_owner_details`, the
nonce (`secrets.token_urlsafe`), the profile home (`Path.resolve`) and the
httpx client, whose request URL is captured so the attach query can be compared
byte for byte.
"""
import __future__
import ast
import hashlib
import hmac
import ipaddress
import json
import sys
import types
from pathlib import Path
from urllib.parse import urlencode, urlsplit

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "shared_session_attach.py"
GATE = "w20_sharedsess_a7d92d"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


# --- seams the fixture drives -------------------------------------------------

CASE = {}
CAPTURED = {}


class _Home:
    """Path stand-in: resolve() is the identity, str() is the fixture's spelling."""

    def __init__(self, s):
        self.s = str(s)

    def resolve(self):
        return self

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s


class HTTPError(Exception):
    pass


class _Resp:
    def __init__(self, body):
        self.body = body

    def raise_for_status(self):
        return None

    def iter_bytes(self):
        for i in range(0, len(self.body), 7):
            yield self.body[i:i + 7]


class _Stream:
    def __init__(self, url):
        self.url = url

    def __enter__(self):
        CAPTURED["url"] = self.url
        handshake = CASE.get("handshake") or {}
        if handshake.get("error"):
            raise HTTPError("handshake refused")
        body = handshake.get("body_raw")
        if body is None:
            body = json.dumps(handshake.get("reply"))
        return _Resp(body.encode("utf-8"))

    def __exit__(self, *exc):
        return False


class _Client:
    def __init__(self, **kw):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def stream(self, method, url):
        return _Stream(url)


httpx_stub = types.ModuleType("httpx")
httpx_stub.HTTPError = HTTPError
httpx_stub.Client = _Client
sys.modules["httpx"] = httpx_stub


def _snapshot(home, strict=True):
    return CASE.get("owners") or []


def _owner_details(session_id, owner):
    return CASE.get("owner_details") or ""


def _hermes_home():
    return _Home(CASE.get("profile_home") or "")


secrets_stub = types.ModuleType("secrets")
secrets_stub.token_urlsafe = lambda n=0: CASE.get("nonce") or ""


# --- live source --------------------------------------------------------------

G = {
    "__name__": "shared_session_attach_live",
    "urlsplit": urlsplit,
    "urlencode": urlencode,
    "ipaddress": ipaddress,
    "hmac": hmac,
    "hashlib": hashlib,
    "json": json,
    "Path": _Home,
    "secrets": secrets_stub,
    "httpx": httpx_stub,
    "get_hermes_home": _hermes_home,
    "active_session_registry_snapshot": _snapshot,
    "session_owner_details": _owner_details,
}


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)


_exec(SRC, {"_local_origin", "discover_attach_url", "configure_tui_attachment"}, G)


def err_of(exc):
    return {"status": "error", "type": type(exc).__name__, "message": str(exc)}


def expected(r):
    global CASE
    CASE = r
    CAPTURED.clear()
    f = r.get("func")
    profile_home = r.get("profile_home") or ""
    if f == "local_origin":
        try:
            host, port = G["_local_origin"](r.get("url"), r.get("scheme") or "http")
            return {"status": "ok", "value": [host, port]}
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    if f == "request":
        try:
            G["discover_attach_url"](r.get("session_id"), registry_home=profile_home)
        except Exception:  # noqa: BLE001 - the request is issued before any verification
            pass
        url = CAPTURED.get("url")
        if not url:
            return {"status": "error", "type": "ValueError", "message": "no request issued"}
        query = url.split("?", 1)[1] if "?" in url else ""
        proof = dict(p.split("=", 1) for p in query.split("&"))["client_proof"]
        return {"status": "ok", "value": {"client_proof": proof, "query": query,
                                          "path": "/api/session-attach"}}
    if f == "discover":
        try:
            return {"status": "ok", "value": G["discover_attach_url"](
                r.get("session_id"), registry_home=profile_home)}
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    if f == "attach":
        env = dict(r.get("env") or {})
        try:
            G["configure_tui_attachment"](env, r.get("session_id"), registry_home=profile_home)
            return env
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"local_origin", "request", "discover", "attach"}
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
