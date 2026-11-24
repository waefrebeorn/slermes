#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/google_chat/setup_files.py (gate w20_setupfiles_7eb3bd).

`handle_setup_files_command` and the real `_run_captured` (stdout capture around
`asyncio.to_thread`) are lifted out of the module's AST and executed as written,
sync or async. The adapter, the OAuth helper module and the catalog translator
are stubs the fixture scripts: the translator returns a `{key, kwargs}` marker so
the reply sequence is comparable without a language catalog, and the helpers
print their output (SystemExit / exception as the fixture asks) exactly like the
real ones.
"""
import __future__
import ast
import asyncio
import contextlib
import io
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "google_chat" / "setup_files.py"
GATE = "w20_setupfiles_7eb3bd"
PKG = "plugins.platforms.google_chat"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

CASE = {}


class _Home:
    def __init__(self, s, exists=False):
        self.s = str(s)
        self._exists = exists

    def exists(self):
        return bool(self._exists)

    def __str__(self):
        return self.s


class _Logger:
    def debug(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass


def _helper():
    h = CASE.get("helper") or {}
    mode = h.get("mode") or "ok"
    if mode == "exit":
        raise SystemExit(0)
    if mode == "error":
        raise RuntimeError(h.get("error") or "")
    print(h.get("output") or "", end="")
    return None


oauth_stub = types.ModuleType(PKG + ".oauth")
oauth_stub._client_secret_path = lambda: _Home("client_secret.json",
                                               CASE.get("client_secret_exists"))
oauth_stub._token_path = lambda key: _Home(CASE.get("token_path") or "",
                                         CASE.get("token_exists"))
oauth_stub.load_user_credentials = lambda key: (
    {"creds": True} if CASE.get("creds_loaded") else None)
oauth_stub.get_auth_url = lambda key: _helper()
oauth_stub.revoke = lambda key: _helper()
oauth_stub.exchange_auth_code = lambda arg, key: _helper()


def _build_service(creds):
    if not CASE.get("build_api_ok", True):
        raise RuntimeError("api build failed")
    return object()


oauth_stub.build_user_chat_service = _build_service

for _name in ("plugins", "plugins.platforms", PKG):
    if _name not in sys.modules:
        _m = types.ModuleType(_name)
        _m.__path__ = []
        sys.modules[_name] = _m
sys.modules[PKG + ".oauth"] = oauth_stub
sys.modules[PKG].oauth = oauth_stub

G = {
    "__name__": PKG + ".setup_files",
    "__package__": PKG,
    "logger": _Logger(),
    "t": lambda key, **kw: {"key": key, "kwargs": kw},
    "_K": "platform.google_chat.setup_files.",
    "_EXITED": object(),
    "asyncio": asyncio,
    "io": io,
    "contextlib": contextlib,
}


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)


_exec(SRC, {"_run_captured", "handle_setup_files_command"}, G)


class Adapter:
    def __init__(self):
        self._user_credentials = None
        self._user_chat_api = None
        self._user_creds_by_email = {}
        self._user_chat_api_by_email = {}
        self.bodies = []

    async def _create_message(self, chat_id, body):
        self.bodies.append(body)


def expected(r):
    global CASE
    CASE = r
    adapter = Adapter()
    kwargs = dict(
        chat_id=r.get("chat_id") or "",
        thread_id=r.get("thread_id"),
        raw_text=r.get("raw_text") or "",
        sender_email=r.get("sender_email"),
    )

    async def _go():
        return await G["handle_setup_files_command"](adapter, **kwargs)

    try:
        consumed = asyncio.run(_go())
    except BaseException as exc:  # noqa: BLE001 - the command can leak IndexError
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}
    return {"status": "ok", "value": {
        "consumed": consumed,
        "replies": adapter.bodies,
        "creds": {
            "shared": adapter._user_credentials is not None,
            "api": adapter._user_chat_api is not None,
            "by_email": sorted(adapter._user_creds_by_email),
        },
    }}


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
        if not isinstance(r, dict) or r.get("func") != "run":
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH run case=%r live-error=%r" % (r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH run case=%r\n  py=%r\n   c=%r" % (r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
