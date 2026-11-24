#!/usr/bin/env python3
"""Differential oracle for the OUTBOUND/CONFIG half of
plugins/platforms/simplex/adapter.py (gate w31_adapter_c93276_p2).

The live adapter module is imported BY FILE PATH (its package __init__ pulls the
gateway stack, and pydantic's compiled extension is blocked on this host); the
shared seam stubs cover that chain.

Seams injected per case, so no socket, no daemon, no filesystem and no clock is
touched:

  _send_command / _send_items  rebound to recorders -- /_send replies and the
                               item arrays the send paths build are the values
                               under test.
  _prepare_image               rebound to (path, thumb) -- the Pillow/ImageMagick
                               conversion is the seam, the item it feeds is not.
  Path.exists                  rebound to True -- the existence probe is the seam.
  _is_sender_authorized        n/a here (no inbound path in this gate).
  _get_scoped_secret           rebound to the case's env map (single-profile
                               install: exactly os.getenv), for check_requirements,
                               validate_config, _env_enablement and __init__.
  time.time                    rebased on the case's now_ms so corrIds are values.

Ported symbols and their live entry points:

  send_cmd       _send_cmd(chat_id, items)            -> target extracted from it
  split_media    SimplexAdapter.send's MEDIA: handling
  is_voice_ext   _VOICE_TAG_EXTS membership
  send_item      send_image / send_document / send_voice captured at _send_items
  corr_id        _make_corr_id / _standalone_send's "snd" corrId
  retry_math     _ws_listener's backoff step
  health_stale   _health_monitor's idleness predicate
  init_config    SimplexAdapter.__init__
  config_gate    check_requirements / validate_config / is_connected
  env_enablement _env_enablement
  get_chat_info  SimplexAdapter.get_chat_info
  list_channels  SimplexAdapter.list_channels
  prepare_image  _prepare_image's needs_png decision
  text_batch_key SimplexAdapter._text_batch_key
"""
import asyncio
import importlib.util
import json
import os
import pathlib
import re
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_adapter_c93276_p2"

_TMPHOME = tempfile.mkdtemp(prefix="w31-simplex-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

_SPEC = importlib.util.spec_from_file_location(
    "w31_simplex_adapter_p2_live", QUARRY / "plugins" / "platforms" / "simplex" / "adapter.py")
live = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(live)

import gateway.platforms._shared as shared  # noqa: E402

ADAPTER = live.SimplexAdapter
WANTED = {
    "send_cmd", "split_media", "is_voice_ext", "send_item", "corr_id", "retry_math",
    "health_stale", "init_config", "config_gate", "env_enablement", "get_chat_info",
    "list_channels", "prepare_image", "text_batch_key",
}

_ENV = {}
_NOW_MS = 0
_REAL_GET_SECRET = live._get_scoped_secret
_REAL_SHARED_SECRET = shared.get_scoped_secret
_REAL_TIME = live.time
_REAL_EXISTS = pathlib.Path.exists


def scoped(name, default=None, **_kw):
    val = _ENV.get(name)
    return default if val is None else val


def install_env(env):
    global _ENV
    _ENV = dict(env or {})
    live._get_scoped_secret = scoped
    shared.get_scoped_secret = scoped


def install_clock(now_ms):
    global _NOW_MS
    _NOW_MS = int(now_ms or 0)
    live.time = types.SimpleNamespace(time=lambda: _NOW_MS / 1000.0)


def restore():
    live._get_scoped_secret = _REAL_GET_SECRET
    shared.get_scoped_secret = _REAL_SHARED_SECRET
    live.time = _REAL_TIME
    pathlib.Path.exists = _REAL_EXISTS


def bare(case):
    adapter = ADAPTER.__new__(ADAPTER)
    adapter.group_allow_from = set()
    adapter._pending_file_transfers = {}
    adapter._pending_text_batches = {}
    return adapter


def make_adapter(extra):
    return ADAPTER(config=types.SimpleNamespace(extra=dict(extra or {})))


# --- ported decisions -------------------------------------------------------

def send_cmd(r):
    items = r.get("items")
    items = items if isinstance(items, list) else []
    cmd = live._send_cmd(r.get("chat_id") or "", items)
    m = re.match(r"^/_send (\S+) json ", cmd)
    assert m, "unexpected /_send shape: %r" % (cmd,)
    return {"target": m.group(1), "items": items}


def split_media(r):
    content = r.get("content") or ""
    return {"media": live.re.findall(r"MEDIA:(\S+)", content),
            "text": live.re.sub(r"MEDIA:\S+", "", content).strip()}


def send_item(r):
    kind = r.get("kind") or "document"
    path = r.get("path") or ""
    caption = r.get("caption")
    captured = []
    adapter = bare(r)
    adapter._prepare_image = lambda p: (p, r.get("thumb") or "")

    async def send_items(chat_id, items, error):
        captured.append(items)
        return types.SimpleNamespace(success=True)

    adapter._send_items = send_items
    pathlib.Path.exists = lambda self: True
    try:
        if kind == "image":
            asyncio.run(adapter.send_image("42", "file://" + path, caption=caption))
        elif kind == "voice":
            asyncio.run(adapter.send_voice("42", path, caption=caption,
                                           duration=r.get("duration") or 0))
        else:
            asyncio.run(adapter.send_document("42", path, caption=caption))
    finally:
        pathlib.Path.exists = _REAL_EXISTS
    return captured[0][0]


def corr_id(r):
    install_clock(r.get("now_ms"))
    try:
        if r.get("counter") is None:
            return "%ssnd-%d" % (live._CORR_PREFIX, int(live.time.time() * 1000))
        adapter = bare(r)
        adapter._max_pending_corr = 200
        adapter._pending_corr_ids = set()
        adapter._corr_counter = int(r["counter"]) - 1
        return adapter._make_corr_id()
    finally:
        live.time = _REAL_TIME


def retry_math(r):
    b = float(r.get("backoff", live.WS_RETRY_DELAY_INITIAL))
    rnd = float(r.get("rnd", 0.0))
    # _ws_listener: sleep(backoff + backoff*0.2*random()); then backoff = min(backoff*2, MAX)
    return {"next": min(b * 2, live.WS_RETRY_DELAY_MAX),
            "sleep": b + b * 0.2 * rnd}


_DELAY_ENV = "HERMES_SIMPLEX_TEXT_BATCH_DELAY"


def init_config(r):
    """__init__ reads the batch delay through os.getenv, the allow/auto flags
    through the scoped secret read; both are seams fed by the case's env."""
    env = r.get("env") or {}
    install_env(env)
    prev = os.environ.get(_DELAY_ENV)
    if _DELAY_ENV in env:
        os.environ[_DELAY_ENV] = str(env[_DELAY_ENV])
    try:
        adapter = make_adapter(r.get("extra"))
    finally:
        if prev is None:
            os.environ.pop(_DELAY_ENV, None)
        else:
            os.environ[_DELAY_ENV] = prev
        restore()
    return {
        "ws_url": adapter.ws_url,
        "auto_accept": adapter.auto_accept,
        "group_allowed": list(dict.fromkeys(sorted(adapter.group_allow_from))),
        "batch_delay": adapter._text_batch_delay_seconds,
        "split_delay": adapter._text_batch_split_delay_seconds,
    }


def config_gate(r):
    install_env(r.get("env"))
    saved = sys.modules.get("websockets", "MISSING")
    sys.modules["websockets"] = types.ModuleType("websockets") if r.get("websockets") else None
    try:
        cfg = types.SimpleNamespace(extra=dict(r.get("extra") or {}))
        val = live.validate_config(cfg)
        return {"check_requirements": live.check_requirements(),
                "validate_config": val,
                "is_connected": live.is_connected(cfg)}
    finally:
        if saved == "MISSING":
            sys.modules.pop("websockets", None)
        else:
            sys.modules["websockets"] = saved
        restore()


def env_enablement(r):
    install_env(r.get("env"))
    try:
        return live._env_enablement()          # None, or {"ws_url": ..., **seed}
    finally:
        restore()


def chat_info(r):
    return asyncio.run(bare(r).get_chat_info(r.get("chat_id") or ""))


def list_channels(r):
    adapter = bare(r)
    adapter._ws = object()
    contacts = r.get("contacts_resp")
    groups = r.get("groups_resp")

    async def send_command(cmd, timeout=30.0):
        if cmd == "/contacts":
            return contacts
        if cmd == "/groups":
            return groups
        return None

    adapter._send_command = send_command
    return asyncio.run(adapter.list_channels())


def prepare_image(r):
    suffix = pathlib.Path(r.get("file_path") or "").suffix.lower()
    return {"needs_png": suffix not in (".png", ".jpg", ".jpeg"), "suffix": suffix}


def text_batch_key(r):
    event = types.SimpleNamespace(source=types.SimpleNamespace(
        platform=types.SimpleNamespace(value=r.get("platform") or "simplex"),
        chat_id=r.get("chat_id") or ""))
    return bare(r)._text_batch_key(event)


def expected(r):
    f = r.get("func")
    if f == "send_cmd":
        return send_cmd(r)
    if f == "split_media":
        return split_media(r)
    if f == "is_voice_ext":
        return (r.get("ext") or "").lower() in live._VOICE_TAG_EXTS
    if f == "send_item":
        return send_item(r)
    if f == "corr_id":
        return corr_id(r)
    if f == "retry_math":
        return retry_math(r)
    if f == "health_stale":
        return float(r.get("elapsed") or 0) > live.HEALTH_CHECK_STALE_THRESHOLD
    if f == "init_config":
        return init_config(r)
    if f == "config_gate":
        return config_gate(r)
    if f == "env_enablement":
        return env_enablement(r)
    if f == "get_chat_info":
        return chat_info(r)
    if f == "list_channels":
        return list_channels(r)
    if f == "prepare_image":
        return prepare_image(r)
    if f == "text_batch_key":
        return text_batch_key(r)
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
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
