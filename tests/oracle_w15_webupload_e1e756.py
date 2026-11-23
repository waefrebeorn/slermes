#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_upload_limit.py (w15_webupload_e1e756).

The live ChatFileUploadLimitMiddleware is driven directly with an in-memory ASGI
scope/receive/send trio: starlette's handful of imported names are stubbed, the
route caps are injected via the live _BODY_MAX_BYTES dict, and the final HTTP
status is compared with the C decision.
"""
import asyncio
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _module(name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    return m


starlette = _module("starlette")
starlette.__path__ = []
ds = _module("starlette.datastructures")


class Headers:
    def __init__(self, scope=None):
        self._d = {}
        for k, v in (scope or {}).get("headers", []):
            self._d[k.decode("latin-1").lower()] = v.decode("latin-1")

    def get(self, key, default=""):
        return self._d.get(key.lower(), default)


ds.Headers = Headers
fp = _module("starlette.formparsers")


class MultiPartException(Exception):
    pass


fp.MultiPartException = MultiPartException
resp = _module("starlette.responses")


class JSONResponse:
    def __init__(self, status_code, content):
        self.status_code = status_code
        self.content = content

    async def __call__(self, scope, receive, send):
        await send({"type": "http.response.start", "status": self.status_code, "headers": []})
        await send({"type": "http.response.body", "body": json.dumps(self.content).encode()})


resp.JSONResponse = JSONResponse
ty = _module("starlette.types")
ty.ASGIApp = object
ty.Message = dict
ty.Receive = object
ty.Scope = dict
ty.Send = object

hermes_cli = _module("hermes_cli")
hermes_cli.__path__ = []
surface = _module("hermes_cli.web_server_surface")
BROWSER_FILES = [True]


def policy(state):
    return types.SimpleNamespace(browser_files=BROWSER_FILES[0])


surface.policy = policy
hc = _module("hermes_constants")
hc.WEBAPP_ATTACHMENT_MAX_BYTES = 16 * 1024 * 1024

SRC = Q + "/hermes_cli/web_upload_limit.py"
spec = importlib.util.spec_from_file_location("w15_webupload_e1e756_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class FakeBody:
    """len() only: avoids allocating the upload bytes just to count them."""
    def __init__(self, n):
        self.n = n

    def __len__(self):
        return self.n


async def run_scope(r):
    BROWSER_FILES[0] = bool(r.get("browser_files", True))
    live._BODY_MAX_BYTES = {
        "/api/chat/file-upload": int(r.get("file_max", 16842752)),
        "/api/chat/image-upload": int(r.get("image_max", 35018112)),
    }
    sent = []

    async def send(msg):
        sent.append(msg)

    chunk = r.get("chunk_total", 0) or 0
    delivered = [False]

    async def receive():
        if delivered[0]:
            return {"type": "http.disconnect"}
        delivered[0] = True
        return {"type": "http.request", "body": FakeBody(int(chunk))}

    async def app(scope, rcv, snd):
        try:
            while True:
                m = await rcv()
                if m["type"] == "http.disconnect":
                    break
        except MultiPartException:
            await snd({"type": "http.response.start", "status": 400, "headers": []})
            await snd({"type": "http.response.body", "body": b"x"})
            return
        await snd({"type": "http.response.start", "status": 200, "headers": []})
        await snd({"type": "http.response.body", "body": b"ok"})

    state = object()
    scope = {
        "path": r.get("path", ""),
        "root_path": r.get("root_path", ""),
        "type": r.get("scope_type", "http"),
        "method": r.get("method", "POST"),
        "app": types.SimpleNamespace(state=state),
        "headers": [],
    }
    if "content_length" in r and r.get("content_length") is not None:
        scope["headers"].append((b"content-length", str(r["content_length"]).encode("latin-1")))

    mw = live.ChatFileUploadLimitMiddleware(app)
    await mw(scope, receive, send)
    starts = [m["status"] for m in sent if m["type"] == "http.response.start"]
    return starts[-1] if starts else 200


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "check":
        continue
    cases += 1
    try:
        py = {"status": asyncio.run(run_scope(r))}
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w15_webupload_e1e756: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
