#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/photon/adapter.py (gate w27_adapter_878564_p3).

Gate 3 compares the NORMALIZE ladder and the outbound payload bodies.  The live
module is imported by file path with the same seam stubs as gate 1.

Seams fed from the fixture:
  * `cache` / `cached` → `_cache_inbound_attachment`'s verdict (a path or None);
    the stub consumes cells in traversal order exactly as the C cursor does.
  * `markdown_env` → PHOTON_MARKDOWN.
  * `_post_send` → an async recorder, so the LIVE `_sidecar_send` /
    `_sidecar_send_poll` run their real body-building ladder with the network
    removed.  `_sidecar_send` is driven with the rich-link attempt FAILING, which
    is the only way the fallback body exists at all; the C plan is that same
    two-attempt sequence.
"""
import asyncio
import importlib.util
import json
import sys
import types
from enum import Enum
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_878564_p3"

ENV: dict = {}


def _install_stubs() -> None:
    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.photon",
                "gateway", "gateway.platforms", "gateway.outbound"):
        m = types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m

    utils = types.ModuleType("utils")
    utils.atomic_json_write = lambda *a, **k: None
    sys.modules["utils"] = utils

    class MessageType(Enum):
        TEXT = "text"
        LOCATION = "location"
        PHOTO = "photo"
        VIDEO = "video"
        AUDIO = "audio"
        VOICE = "voice"
        DOCUMENT = "document"
        STICKER = "sticker"
        COMMAND = "command"

    ev = types.ModuleType("gateway.platforms.event")
    ev.MessageType = MessageType

    class MessageEvent:
        def __init__(self, **kw):
            self.__dict__.update(kw)

    ev.MessageEvent = MessageEvent
    sys.modules["gateway.platforms.event"] = ev

    class SendResult:
        def __init__(self, success=False, message_id=None, error=None,
                     raw_response=None, retryable=False):
            self.success = success
            self.message_id = message_id
            self.error = error
            self.raw_response = raw_response
            self.retryable = retryable

    base = types.ModuleType("gateway.platforms.base")

    class BasePlatformAdapter:
        def __init__(self, config=None, platform=None):
            self.config = config
            self.platform = platform

        @staticmethod
        def _is_retryable_error(error):
            return False

        @staticmethod
        def validate_media_delivery_path(p):
            return str(p) if p else None

    base.BasePlatformAdapter = BasePlatformAdapter
    base.SendResult = SendResult
    sys.modules["gateway.platforms.base"] = base

    shared = types.ModuleType("gateway.platforms._shared")
    shared.coerce_port = lambda v, d: d
    shared.extra_or_secret = lambda extra, k, e, d="", **kw: ENV.get(e, d)
    shared.get_scoped_secret = lambda k, d=None, **kw: ENV.get(k, d)
    shared.seed_extra_from_env = lambda *a, **k: {}
    shared.send_error = lambda msg, **k: {"error": str(msg)}
    sys.modules["gateway.platforms._shared"] = shared

    st = types.ModuleType("gateway.platforms.send_targets")
    st.regex_stripped_parser = lambda r: (
        lambda ref: (ref.strip(), None) if r.fullmatch(ref) else None)
    sys.modules["gateway.platforms.send_targets"] = st

    cfg = types.ModuleType("gateway.config")

    class Platform:
        def __init__(self, n):
            self.value = n

    class PlatformConfig:
        pass

    cfg.Platform = Platform
    cfg.PlatformConfig = PlatformConfig
    sys.modules["gateway.config"] = cfg

    hc = types.ModuleType("hermes_constants")
    hc.find_node_executable = lambda n: None
    hc.with_hermes_node_path = lambda: {}
    sys.modules["hermes_constants"] = hc

    auth = types.ModuleType("plugins.platforms.photon.auth")
    auth.load_project_credentials = lambda: (None, None)
    sys.modules["plugins.platforms.photon.auth"] = auth

    sp = types.ModuleType("plugins.platforms.photon.sidecar_paths")
    sp._NPM_ERROR_LOG_MAX_CHARS = 4000
    sp._lock_newer_than_install = lambda *a, **k: False
    sp._npm_error_log = lambda *a, **k: None
    sp._sidecar_dir = lambda *a, **k: None
    sp.dir_writable = lambda *a, **k: False
    sys.modules["plugins.platforms.photon.sidecar_paths"] = sp

    gst = types.ModuleType("gateway.status")
    gst._pid_exists = lambda p: False
    sys.modules["gateway.status"] = gst

    rss = types.ModuleType("gateway.rich_sent_store")
    rss.record_async = lambda *a, **k: None
    rss.lookup = lambda *a, **k: None
    sys.modules["gateway.rich_sent_store"] = rss

    pm = types.ModuleType("pm")
    pm.lazy_installs_allowed = lambda: False
    sys.modules["pm"] = pm


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, str(path))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


_install_stubs()
helpers = _load("gateway.platforms.helpers", QUARRY / "gateway" / "platforms" / "helpers.py")
live = _load("plugins.platforms.photon.adapter",
             QUARRY / "plugins" / "platforms" / "photon" / "adapter.py")

ADAPTER = live.PhotonAdapter
SendResult = sys.modules["gateway.platforms.base"].SendResult

WANTED = {"normalize_binary_payload", "normalize_group_content", "normalize_content",
          "attachment_body", "attachment_label", "send_plan", "send_poll_plan"}

_CACHE: list = []


def _cache_seam(content, name, mime, *, force_audio=False):
    """Stands in for the media-cache write: the fixture's next cell."""
    if _CACHE:
        return _CACHE.pop(0)
    return None


live._cache_inbound_attachment = _cache_seam


def _md_env(req):
    ENV.pop("PHOTON_MARKDOWN", None)
    if req.get("markdown_env") is not None:
        ENV["PHOTON_MARKDOWN"] = req["markdown_env"]


def _fill_cache(req, key="cache"):
    _CACHE.clear()
    raw = req.get(key)
    if isinstance(raw, list):
        _CACHE.extend(raw)


def _normalized(t):
    text, mtype, urls, types_ = t
    return {"text": text, "type": mtype.value if hasattr(mtype, "value") else mtype,
            "media_urls": urls, "media_types": types_}


def _recorder(attrs):
    calls = []

    async def _post_send(path, body, *, structured=False, sent_text=None):
        calls.append({"path": path, "body": body})
        # the rich-link attempt always fails so the fallback body is produced
        if path == "/send-richlink":
            return SendResult(success=False, error="rich-link down")
        return SendResult(success=True, message_id="m1")

    return _post_send, calls


def _bare():
    a = ADAPTER.__new__(ADAPTER)
    a._sent_message_ids = {}
    a._last_inbound_by_chat = {}
    a._recent_richlinks_by_chat = {}
    return a


def expected(r):
    f = r.get("func")
    _md_env(r)
    if f == "normalize_binary_payload":
        _CACHE.clear()
        if r.get("cached"):
            _CACHE.append(r["cached"])
        return _normalized(live._normalize_binary_payload(r.get("payload") or {}))
    if f == "normalize_group_content":
        _fill_cache(r)
        return _normalized(live._normalize_group_content(r.get("content") or {}))
    if f == "normalize_content":
        _fill_cache(r)
        return _normalized(live._normalize_content(r.get("content") or {}))
    if f == "attachment_body":
        return live._attachment_body(
            r.get("space_id") or "", r.get("safe_path") or "",
            kind=r.get("kind") if r.get("kind") is not None else "attachment",
            name=r.get("name"), mime_type=r.get("mime_type"), caption=r.get("caption"))
    if f == "attachment_label":
        return live._attachment_label(r.get("kind") or "", r.get("name") or "")
    if f == "send_plan":
        a = _bare()
        post, calls = _recorder(None)
        a._post_send = post
        asyncio.run(a._sidecar_send(r.get("space_id") or "", r.get("text") or "",
                                    richlink=bool(r.get("richlink")),
                                    markdown=bool(r.get("markdown", True))))
        attempts = [{"path": c["path"], "body": c["body"]} for c in calls]
        rich = attempts[0]["body"]["url"] if attempts and attempts[0]["path"] == "/send-richlink" else None
        return {"sent_text": r.get("text") or "", "rich_url": rich, "attempts": attempts}
    if f == "send_poll_plan":
        a = _bare()
        post, calls = _recorder(None)
        a._post_send = post
        res = asyncio.run(a._sidecar_send_poll(r.get("space_id") or "",
                                               r.get("title") or "",
                                               r.get("options") or []))
        if calls:
            return {"ok": True, "body": calls[0]["body"], "error": None,
                    "sent_text": calls[0]["body"].get("title")}
        return {"ok": False, "body": None, "error": res.error, "sent_text": None}
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
    if cases == 0:
        print("VACUOUS %s: 0 cases reached the oracle (empty fixture, or the harness "
              "produced no parseable lines)" % GATE, file=sys.stderr)
        sys.exit(2)
    if cases < 10:
        print("VACUOUS %s: only %d cases ran (< 10); a gate needs real coverage"
              % (GATE, cases), file=sys.stderr)
        sys.exit(2)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
