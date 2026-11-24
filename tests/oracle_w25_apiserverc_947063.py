#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_cron_jobs.py
(gate w25_apiserverc_947063).

aiohttp is not installed in this interpreter, so ``aiohttp.web`` is replaced by
a two-field stand-in (``json_response`` -> an object with ``status``/``data``).
Everything else is live:

  * ``handle_update_job`` runs under asyncio against a fake adapter and a fake
    facade, and the sanitized dict the store call receives is the comparison
    value (the whitelist comprehension is inside that handler);
  * ``_invalid_update`` is called directly with a fixed prompt-scanner verdict;
  * ``job_response`` is called with a store function that returns or raises as
    the fixture says, and the notify call is observed, not re-implemented;
  * ``_UPDATE_ALLOWED_FIELDS`` is read from api_server.py's AST (that module
    cannot be imported without aiohttp).
"""
import ast
import asyncio
import importlib
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_apiserverc_947063"

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


class _Resp:
    def __init__(self, data, status=200):
        self.data, self.status = data, status


def _install_fake_aiohttp():
    try:
        import aiohttp  # noqa: F401
        return
    except ImportError:
        pass
    fake = types.ModuleType("aiohttp")
    fake.web = types.SimpleNamespace(
        json_response=lambda data, status=200: _Resp(data, status),
        Request=object, Response=_Resp)
    sys.modules["aiohttp"] = fake


_install_fake_aiohttp()
live = importlib.import_module("gateway.platforms.api_server_cron_jobs")  # noqa: E402

WANTED = {"allowed_fields", "sanitize", "invalid_update", "job_status"}


class _Recorder:
    def __init__(self):
        self.notified = False
        self.sanitized = None


class _Adapter:
    def __init__(self, allowed, max_len=200, prompt_error=None):
        self._UPDATE_ALLOWED_FIELDS = set(allowed)
        self._MAX_NAME_LENGTH = max_len
        self._prompt_error = prompt_error

    def _cron_request_guard(self, request, need_job_id=False):
        return "job-1", None

    def _validate_cron_prompt(self, prompt):
        if self._prompt_error:
            return _Resp({"error": self._prompt_error}, 400)
        return None


class _Request:
    def __init__(self, body):
        self._body = body

    async def json(self):
        return self._body


def _install_facade(rec):
    mod = types.ModuleType("gateway.platforms.api_server")
    mod._notify_cron_provider_jobs_changed = lambda: setattr(rec, "notified", True)

    def _cron_update(job_id, sanitized):
        rec.sanitized = sanitized
        return dict(sanitized, id=job_id)

    mod._cron_update = _cron_update
    pkg = sys.modules["gateway.platforms"]
    previous = (sys.modules.get("gateway.platforms.api_server"),
                getattr(pkg, "api_server", None))
    sys.modules["gateway.platforms.api_server"] = mod
    setattr(pkg, "api_server", mod)
    return previous


def _restore_facade(previous):
    pkg = sys.modules["gateway.platforms"]
    mod, attr = previous
    if mod is None:
        sys.modules.pop("gateway.platforms.api_server", None)
    else:
        sys.modules["gateway.platforms.api_server"] = mod
    if attr is None:
        try:
            delattr(pkg, "api_server")
        except AttributeError:
            pass
    else:
        setattr(pkg, "api_server", attr)


def allowed_fields(_req):
    """_UPDATE_ALLOWED_FIELDS from the api_server.py source (import-blocked)."""
    src = (QUARRY / "gateway" / "platforms" / "api_server.py").read_text(encoding="utf-8")
    for node in ast.walk(ast.parse(src)):
        targets = []
        if isinstance(node, ast.Assign):
            targets = node.targets
        elif isinstance(node, ast.AnnAssign):
            targets = [node.target]
        for target in targets:
            name = getattr(target, "id", None) or getattr(target, "attr", None)
            if name == "_UPDATE_ALLOWED_FIELDS":
                return sorted(ast.literal_eval(node.value))
    raise AssertionError("_UPDATE_ALLOWED_FIELDS not found in api_server.py")


def sanitize(req):
    allowed = req.get("allowed")
    rec = _Recorder()
    adapter = _Adapter(allowed if isinstance(allowed, list) else allowed_fields(req))
    previous = _install_facade(rec)
    try:
        asyncio.run(live.handle_update_job(adapter, _Request(req.get("body"))))
    finally:
        _restore_facade(previous)
    # Rejected before the store call (non-object body, or nothing whitelisted).
    return rec.sanitized


def invalid_update(req):
    adapter = _Adapter([], max_len=req.get("max_name_length", 200),
                       prompt_error=req.get("prompt_error"))
    resp = live._invalid_update(adapter, req.get("sanitized"))
    if resp is None:
        return None
    return {"status": resp.status, "error": resp.data.get("error")}


def job_status(req):
    rec = _Recorder()
    err = req.get("error")
    if isinstance(err, dict):
        message = err.get("message")

        def fn(job_id):
            raise (ValueError(message) if err.get("kind") == "invalid"
                   else RuntimeError(message))
    else:
        job = req.get("job")

        def fn(job_id):
            return job

    adapter = types.SimpleNamespace(
        _cron_error_response=lambda exc: _Resp({"error": "internal error"}, 500))
    previous = _install_facade(rec)
    try:
        resp = live.job_response(adapter, fn, "job-1", notify=bool(req.get("notify")),
                                 invalid_input=ValueError)
    finally:
        _restore_facade(previous)
    return {"status": resp.status, "body": resp.data, "notified": rec.notified}


def expected(r):
    f = r.get("func")
    if f == "allowed_fields":
        return allowed_fields(r)
    if f == "sanitize":
        return sanitize(r)
    if f == "invalid_update":
        return invalid_update(r)
    if f == "job_status":
        return job_status(r)
    raise AssertionError("unknown func %r" % (f,))


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
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
