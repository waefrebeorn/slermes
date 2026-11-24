#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/dingtalk/adapter.py, gate p2.

The live adapter is driven through the seams it already has, so the values
compared are the ones the real code produces — never a paraphrase of it:

  * ``send()`` over a recording fake httpx client: the session-webhook body
    (``content[:MAX_MESSAGE_LENGTH]`` + ``_normalize_markdown``) is read back
    from the recorded POST;
  * ``_standalone_send()`` with ``httpx.AsyncClient`` swapped for the same
    recorder: the cron/robot text body;
  * ``send_image()``: the composed "caption + image markdown" reply;
  * ``send_image_file()`` / ``send_document()``: the _NO_LOCAL_UPLOAD refusals;
  * ``_run_stream_loop()``: a scripted stream client raises the fixture's
    exception sequence, ``asyncio.sleep`` is shimmed to record the delay and
    stop the loop, and the circuit-breaker ERROR record is counted — so
    ``delay`` and ``tripped`` are observed, not reimplemented.  The two
    counters beside them are recounted from the script;
  * ``_get_valid_webhook()`` with ``datetime`` pinned to the fixture's ms;
  * ``_credentials()`` / ``_is_connected()`` with the scoped env rung set from
    the fixture (get_scoped_secret reads the profile env);
  * ``_nested_allowed_users()``;
  * ``_SdkLogGuard.filter()``: the record is filtered and the resulting key is
    read out of ``_seen`` (the 500-char truncation is the guard's own).

Nothing here posts to the network: httpx is never called for real, the
websocket client is a stub class, and no task outlives the case.
"""
import asyncio
import json
import logging
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_91f75f_p2"

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(QUARRY.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.dingtalk import adapter as live  # noqa: E402

ADAPTER = live.DingTalkAdapter
WANTED = {"webhook_payload", "standalone_payload", "image_content",
          "no_local_upload", "reconnect_plan", "webhook_valid", "credentials",
          "is_connected", "nested_allowed_users", "sdk_log_key"}

ENV_KEYS = ("DINGTALK_CLIENT_ID", "DINGTALK_CLIENT_SECRET", "DINGTALK_WEBHOOK_URL")
WEBHOOK = "https://api.dingtalk.com/robot/send?access_token=fixture"

for _k in ENV_KEYS:
    os.environ.pop(_k, None)


class _Cfg:
    def __init__(self, extra=None):
        self.extra = extra if isinstance(extra, dict) else {}

    platform = live.Platform.DINGTALK


class _Resp:
    status_code = 200
    text = "ok"

    def json(self):
        return {"errcode": 0, "errmsg": "ok"}

    def raise_for_status(self):
        return None


class _Recorder:
    """Stands in for httpx.AsyncClient: records every POST body."""

    def __init__(self, *a, **k):
        self.posts = []

    async def __aenter__(self):
        return self

    async def __aexit__(self, *a):
        return False

    async def post(self, url, json=None, timeout=None):
        self.posts.append({"url": url, "json": json})
        return _Resp()


class _RecorderClass:
    last = None

    def __new__(cls, *a, **k):
        cls.last = _Recorder(*a, **k)
        return cls.last


def bare(extra=None):
    a = ADAPTER.__new__(ADAPTER)
    a.config = _Cfg(extra)
    a.platform = live.Platform.DINGTALK
    a._mention_patterns = []
    a._allowed_users = set()
    a._session_webhooks = {}
    a._message_contexts = {}
    a._card_template_id = None
    a._card_sdk = None
    a._streaming_cards = {}
    a._done_emoji_fired = set()
    a._bg_tasks = set()
    a._client_id = ""
    a._robot_code = ""
    a._http_client = _Recorder()
    return a


# --- payload shaping -------------------------------------------------

def webhook_payload(r):
    content = r.get("content")
    if not isinstance(content, str):
        raise TypeError("content must be str")
    a = bare()
    asyncio.run(a.send("c1", content, metadata={"session_webhook": WEBHOOK}))
    return a._http_client.posts[-1]["json"]


def standalone_payload(r):
    real = live.httpx.AsyncClient if live.httpx else None
    import httpx as _httpx
    _httpx.AsyncClient = _RecorderClass
    try:
        asyncio.run(live._standalone_send(_Cfg({"webhook_url": WEBHOOK}), "c1",
                                          r.get("message")))
    finally:
        _httpx.AsyncClient = real
    post = _RecorderClass.last.posts[-1]
    return post["json"]


def image_content(r):
    a = bare()
    asyncio.run(a.send_image("c1", r.get("image_url"), r.get("caption"),
                             metadata={"session_webhook": WEBHOOK}))
    # captions here normalise to themselves, so the posted text is the composed reply
    return a._http_client.posts[-1]["json"]["markdown"]["text"]


def no_local_upload(r):
    a = bare()
    kind = r.get("kind")
    if kind == "image":
        res = asyncio.run(a.send_image_file("c1", "/tmp/x.png"))
    elif kind == "file":
        res = asyncio.run(a.send_document("c1", "/tmp/x.pdf"))
    else:
        raise AssertionError("unknown kind %r" % (kind,))
    return res.error


# --- retry math ------------------------------------------------------

class _EA(Exception):
    pass


class _EB(Exception):
    pass


class _EC(Exception):
    pass


class _RaisingClient:
    """Stream client whose start() raises the scripted exception sequence."""

    def __init__(self, errors, adapter):
        self._errors, self._adapter, self._k = list(errors), adapter, 0

    async def start(self):
        if self._k < len(self._errors):
            exc = self._errors[self._k]
            self._k += 1
            raise exc
        self._adapter._running = False      # never spin forever
        raise RuntimeError("script exhausted")


class _AShim:
    """live.asyncio replacement: records sleeps, keeps CancelledError."""

    CancelledError = asyncio.CancelledError

    def __init__(self, on_sleep):
        self._on_sleep = on_sleep

    async def sleep(self, delay):
        self._on_sleep(delay)


class _Collect(logging.Handler):
    def __init__(self):
        super().__init__(level=logging.DEBUG)
        self.records = []

    def emit(self, record):
        self.records.append(record)


def _drive(errors):
    """Run the live reconnect loop over the scripted types; return sleeps + trip."""
    a = bare()
    a._running = True
    a._stream_client = _RaisingClient([cls("boom") for cls in errors], a)
    delays, errors_seen, handler = [], [], _Collect()
    total = len(errors)

    def on_sleep(delay):
        # snapshot AFTER this step's error handling, BEFORE the loop ends
        delays.append(delay)
        errors_seen.append(sum(1 for rec in handler.records if rec.levelno >= logging.ERROR))
        if len(delays) >= total:
            a._running = False

    shim, real = _AShim(on_sleep), live.asyncio
    live.asyncio = shim
    live.logger.addHandler(handler)
    try:
        asyncio.run(a._run_stream_loop())
    finally:
        live.asyncio = real
        live.logger.removeHandler(handler)
    before = errors_seen[-2] if len(errors_seen) > 1 else 0
    tripped = bool(errors_seen) and errors_seen[-1] > before
    return delays, tripped


def reconnect_plan(r):
    c = int(r.get("consecutive") or 0)
    same = bool(r.get("same_type"))
    idx = int(r.get("backoff_idx") or 0)
    if c < 0 or idx < c:
        raise AssertionError("unreachable state (%d,%d)" % (c, idx))
    # walk the loop into (consecutive=c, backoff_idx=idx, last type B)
    script = [_EA] * (idx - c) + [_EB] * c
    final = _EB if same else _EC
    delays, tripped = _drive(script + [final])
    after = c + 1 if same else 1
    return {
        "consecutive_same_error": after,
        "delay": delays[-1],
        "tripped": tripped,
        "backoff_idx": idx if after > 5 else idx + 1,
    }


# --- webhook expiry --------------------------------------------------

class _Now:
    def __init__(self, ms):
        self._ms = ms

    def timestamp(self):
        return self._ms / 1000.0


class _FakeDatetime:
    ms = 0.0

    @classmethod
    def now(cls, tz=None):
        return _Now(cls.ms)


_REAL_DT = live.datetime


def webhook_valid(r):
    info = r.get("info")
    a = bare()
    if isinstance(info, dict):
        a._session_webhooks["c1"] = (info.get("webhook"), info.get("expired_ms"))
    live.datetime = _FakeDatetime
    _FakeDatetime.ms = float(r.get("now_ms") or 0)
    try:
        got = a._get_valid_webhook("c1")
    finally:
        live.datetime = _REAL_DT
    if got is None:
        return None
    return {"webhook": got[0], "expired_ms": got[1]}


# --- credentials -----------------------------------------------------

def _set_env(env_id, env_secret):
    for key, val in (("DINGTALK_CLIENT_ID", env_id), ("DINGTALK_CLIENT_SECRET", env_secret)):
        if val is None:
            os.environ.pop(key, None)
        else:
            os.environ[key] = val if isinstance(val, str) else str(val)


def credentials(r):
    extra = r.get("extra")
    _set_env(r.get("env_id"), r.get("env_secret"))
    try:
        return list(live._credentials(extra if isinstance(extra, dict) else None))
    finally:
        _set_env(None, None)


def is_connected(r):
    extra = r.get("extra")
    _set_env(r.get("env_id"), r.get("env_secret"))
    try:
        return live._is_connected(_Cfg(extra))
    finally:
        _set_env(None, None)


# --- yaml allowlist --------------------------------------------------

def nested_allowed_users(r):
    return live._nested_allowed_users(r.get("yaml_cfg"), r.get("dingtalk_cfg"))


# --- sdk log guard key -----------------------------------------------

def sdk_log_key(r):
    msg, exc_type = r.get("msg"), r.get("exc_type")
    exc_info = None
    if exc_type is not None:
        cls = type(str(exc_type), (Exception,), {})
        exc_info = (cls, cls("x"), None)
    rec = logging.LogRecord("dingtalk_stream.client", int(r.get("level") or 0),
                            "p", 1, msg, None, exc_info)
    guard = live._SdkLogGuard()
    guard.filter(rec)
    if not guard._seen:
        return None
    key = next(iter(guard._seen))
    return [key[0], key[1], key[2]]


CALLS = {
    "webhook_payload": webhook_payload,
    "standalone_payload": standalone_payload,
    "image_content": image_content,
    "no_local_upload": no_local_upload,
    "reconnect_plan": reconnect_plan,
    "webhook_valid": webhook_valid,
    "credentials": credentials,
    "is_connected": is_connected,
    "nested_allowed_users": nested_allowed_users,
    "sdk_log_key": sdk_log_key,
}


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
            py = CALLS[r["func"]](r)
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
