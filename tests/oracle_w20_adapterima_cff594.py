#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter_image_batch.py
(gate w20_adapterima_cff594).

`collect_image_uploads` is lifted out of the module's AST and executed as the
async function it is. The filesystem, the SSRF predicate, the HTTP client and
the bounded body reader are stubs the fixture scripts, and the real
`urllib.parse.unquote` / `os.path.basename` are handed in exactly as the adapter
hands them in, so the batch shaping is compared against live code.
"""
import __future__
import ast
import asyncio
import json
import ntpath
import sys
import types
from pathlib import Path
from urllib.parse import unquote

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "slack" / "adapter_image_batch.py"
GATE = "w20_adapterima_cff594"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

CASE = {}


class _Logger:
    def warning(self, *a, **k):
        pass

    def debug(self, *a, **k):
        pass


class _Resp:
    def __init__(self, spec):
        self.spec = spec
        self.headers = {"content-type": spec.get("content_type", "")}

    def raise_for_status(self):
        if (self.spec.get("fail") or "") == "status":
            raise RuntimeError("status 500")


class _Stream:
    def __init__(self, spec):
        self.spec = spec

    async def __aenter__(self):
        if (self.spec.get("fail") or "") == "stream":
            raise RuntimeError("connection reset")
        return _Resp(self.spec)

    async def __aexit__(self, *exc):
        return False


class _Client:
    def __init__(self, **kw):
        pass

    async def __aenter__(self):
        return self

    async def __aexit__(self, *exc):
        return False

    def stream(self, method, url):
        spec = (CASE.get("downloads") or {}).get(url) or {}
        return _Stream(spec)


def _client_factory(**kw):
    return _Client(**kw)


async def _read_body(response, media_type="image"):
    if (response.spec.get("fail") or "") == "body" or not response.spec.get("ok"):
        raise RuntimeError("body over the media limit")
    return (response.spec.get("content") or "").encode("utf-8")


os_stub = types.ModuleType("os")
os_stub.path = types.SimpleNamespace(
    exists=lambda p: p in set(CASE.get("exists") or []),
    basename=ntpath.basename,
)

G = {
    "__name__": "adapter_image_batch_live",
    "logger": _Logger(),
    "asyncio": asyncio,
    "os": os_stub,
    "read_httpx_body_with_limit": _read_body,
    "ssrf_redirect_guard": lambda *a, **k: None,
}


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


_exec(SRC, {"collect_image_uploads", "_IMAGE_CT_EXTS"}, G)


def plain(v):
    if isinstance(v, bytes):
        return v.decode("utf-8", "replace")
    if isinstance(v, dict):
        return {k: plain(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [plain(x) for x in v]
    return v


def expected(r):
    global CASE
    CASE = r
    f = r.get("func")
    if f == "ext_for":
        ct = r.get("content_type") or ""
        return next((e for k, e in (("jpeg", "jpg"), ("jpg", "jpg"), ("gif", "gif"),
                                    ("webp", "webp")) if k in ct), "png")
    if f == "unquote":
        return unquote(r.get("value") or "")
    if f == "basename":
        return ntpath.basename(r.get("path") or "")
    if f == "collect":
        chunk = [tuple(pair) for pair in (r.get("chunk") or [])]
        unsafe = set(r.get("unsafe") or [])

        async def _go():
            return await G["collect_image_uploads"](
                chunk, unquote, (lambda u: u not in unsafe), _client_factory,
                lambda u: "safe-url")

        uploads, alts, refused = asyncio.run(_go())
        return {"status": "ok", "value": {"file_uploads": plain(uploads),
                                          "initial_comment_parts": plain(alts),
                                          "refused": plain(refused)}}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"collect", "ext_for", "unquote", "basename"}
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
