#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/uploads.py
(gate w21_uploads_23e65f).

The live router module is imported as-is and `upload_chat_file` runs its real
body: the byte cap, the 413 detail wording, the staged-file receipt, the
`file.filename or "attachment"` fallback and the conditional descriptor. The
spool (FastAPI UploadFile/HTTPException/APIRouter) and the staged-upload writer
are transport/config seams and are scripted from the fixture.
"""
import asyncio
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "web_routers" / "uploads.py"
GATE = "w21_uploads_23e65f"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

CASE = {}


class HTTPException(Exception):
    def __init__(self, status_code=0, detail=""):
        super().__init__(detail)
        self.status_code = status_code
        self.detail = detail


class APIRouter:
    def post(self, *args, **kwargs):
        def register(func):
            return func
        return register


class UploadFile:
    pass


def File(default=None, **kwargs):  # noqa: N802 - FastAPI's spelling
    return default


class FakeSpool:
    def __init__(self, size):
        self.size = size

    def seek(self, offset, whence=0):
        return self.size


class FakeUpload:
    def __init__(self, case):
        self.file = FakeSpool(case.get("size") or 0)
        self.filename = case.get("filename")
        self.closed = False

    async def close(self):
        self.closed = True


def stub(name, **attrs):
    mod = types.ModuleType(name)
    mod.__dict__.update(attrs)
    sys.modules[name] = mod
    return mod


class FakeTarget:
    """The staged path as the receipt prints it (`str(target)`), platform-neutral."""

    def __init__(self, text):
        self.text = text

    def __str__(self):
        return self.text


def resolve_upload_home(profile):
    return FakeTarget(CASE.get("path") or "/tmp/upload-home")


def publish_staged_upload(spool, home, filename):
    PUBLISHED.append(filename)
    return FakeTarget(CASE.get("path") or "/tmp/upload-home/uploads/web-1")


def staged_upload_descriptor(target, home):
    return CASE.get("descriptor")


PUBLISHED = []

for _pkg in ("hermes_cli", "hermes_cli.web_routers"):
    if _pkg not in sys.modules:
        stub(_pkg)
stub("fastapi", APIRouter=APIRouter, File=File, HTTPException=HTTPException,
     UploadFile=UploadFile)
stub("hermes_cli.staged_uploads", publish_staged_upload=publish_staged_upload,
     resolve_upload_home=resolve_upload_home,
     staged_upload_descriptor=staged_upload_descriptor)

_spec = importlib.util.spec_from_file_location("web_routers_uploads_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["web_routers_uploads_live"] = live
_spec.loader.exec_module(live)


def expected(r):
    global CASE
    CASE = r
    del PUBLISHED[:]
    upload = FakeUpload(r)
    try:
        result = asyncio.run(live.upload_chat_file(file=upload, profile=r.get("profile")))
    except HTTPException as exc:
        return {"error": {"status_code": exc.status_code, "detail": exc.detail}}
    result = dict(result)
    result["published_name"] = PUBLISHED[0] if PUBLISHED else None
    return result


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
        if not isinstance(r, dict) or r.get("func") != "upload":
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - a live raise is a harness bug
            mism += 1
            print("MISMATCH upload case=%r live-error=%r" % (r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH upload case=%r\n  py=%r\n   c=%r"
                  % (r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
