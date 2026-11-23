#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_images.py
(w8_sessionimag_8b2bc8 gate).

The live module is loaded via spec. The real tui_gateway.prompt_attachments
module is loaded too (with its handler-registry import stubbed), so the real
data-URL/base64 decode and image-ext sniff run; only stage_image_bytes is
replaced by a deterministic stand-in (the real one uses a random mkstemp name).
authorize is injected to raise a fixture-selected AuthorityRefusal.
"""
import asyncio
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class AuthorityRefusal(Exception):
    pass


pkg = _mod("persistence"); pkg.__path__ = []
core = _mod("persistence.core"); core.__path__ = []
_mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal, STORE_FAILURES=())

tui = _mod("tui_gateway"); tui.__path__ = []


class _HR:
    def __init__(self, *a, **k):
        pass

    def __getattr__(self, name):
        return lambda *a, **k: (lambda f: f)


_mod("tui_gateway.method_ctx", HandlerRegistry=_HR, bind_module=lambda *a, **k: None)

# Real prompt_attachments, loaded under its canonical name.
pa_path = Q + "/tui_gateway/prompt_attachments.py"
pa_spec = importlib.util.spec_from_file_location("tui_gateway.prompt_attachments", pa_path)
pa = importlib.util.module_from_spec(pa_spec)
sys.modules["tui_gateway.prompt_attachments"] = pa
pa_spec.loader.exec_module(pa)


class _FakePath:
    def __init__(self, name):
        self._name = name

    @property
    def name(self):
        return self._name

    def __str__(self):
        return "/stage/" + self._name


STORAGE = [True]


def _fake_stage(directory, img_bytes, ext, *, prefix):
    if not STORAGE[0]:
        raise OSError("staging unavailable")
    return _FakePath("upload_stub" + ext)


pa.stage_image_bytes = _fake_stage

AUTHZ = [""]


def _authorize(connection, ref, params, op):
    if AUTHZ[0]:
        raise AuthorityRefusal(AUTHZ[0])


gpkg = _mod("gateway"); gpkg.__path__ = []
_mod("gateway.session_busy_controls", authorize=_authorize)

SRC = Q + "/gateway/session_images.py"
spec = importlib.util.spec_from_file_location("sessimg_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sessimg_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func != "attach":
        continue
    try:
        AUTHZ[0] = r.get("authorize_error", "") or ""
        STORAGE[0] = bool(r.get("storage_ok", True))
        ref = types.SimpleNamespace(session_id=r.get("session_id"))
        subs = [r.get("session_id")] if r.get("subscribed", False) else []
        conn = types.SimpleNamespace(
            subscriptions=subs,
            authority=types.SimpleNamespace(profile_id=r.get("profile_id", "default")),
        )
        params = r.get("params", {})
        result = asyncio.run(live.attach_bytes(conn, ref, params))
        py = {"ok": True, "result": result}
    except AuthorityRefusal as exc:
        py = {"ok": False, "error": exc.args[0] if exc.args else ""}
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w8_sessionimag_8b2bc8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
