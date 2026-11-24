#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/photon/adapter.py (gate w27_adapter_878564_p5).

Gate 5 compares the MENTION and PRESENCE gates.  Same by-path import + seam
stubs as gate 1; additionally:

  * `voice_mention_gate_enabled` is replaced by a seam answering the fixture's
    `voice_gate_enabled` flag (in production it reads config.extra, which this
    harness does not build).
  * `time` is a namespace whose `time()`/`monotonic()` return the fixture's
    `now`, so the 30 s rich-link preview window is deterministic.
  * PHOTON_MARKDOWN / PHOTON_REACTIONS / PHOTON_SIDECAR_AUTOSTART /
    PHOTON_REQUIRE_MENTION come from the fixture through `_get_scoped_secret`.

The mention matcher is compared against the LIVE compiled patterns produced by
the real `compile_mention_patterns` (loaded from helpers.py by path), with the
fixtures restricted to the wake-word grammar the adapter ships by default.
"""
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
GATE = "w27_adapter_878564_p5"

ENV: dict = {}


def _install_stubs() -> None:
    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.photon",
                "gateway", "gateway.platforms", "gateway.outbound"):
        m = types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m

    utils = types.ModuleType("utils")
    utils.atomic_json_write = lambda *a, **k: None
    utils.is_truthy_value = lambda v, default=False: default
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
REAL_TIME = live.time
WANTED = {"mention_pattern_texts", "mention_match", "clean_mention_text",
          "gate_stt_chat", "require_mention_flag", "markdown_enabled",
          "reactions_enabled", "autostart_enabled",
          "is_recent_richlink_preview", "record_recent_richlink"}


def _patterns(raw):
    """_compile_mention_patterns(raw): raw None -> defaults."""
    return ADAPTER._compile_mention_patterns(raw)


def _bare(patterns=None):
    a = ADAPTER.__new__(ADAPTER)
    a._mention_patterns = _patterns(patterns)
    a._sent_message_ids = {}
    a._last_inbound_by_chat = {}
    a._recent_richlinks_by_chat = {}
    a._pending_fffc = {}
    return a


def expected(r):
    f = r.get("func")
    if f == "mention_pattern_texts":
        return [p.pattern for p in _patterns(r.get("raw"))]
    if f == "mention_match":
        return _bare(r.get("patterns"))._message_matches_mention_patterns(r.get("text") or "")
    if f == "clean_mention_text":
        return _bare(r.get("patterns"))._clean_mention_text(r.get("text") or "")
    if f == "gate_stt_chat":
        a = _bare()
        a.require_mention = bool(r.get("require_mention"))
        a.config = types.SimpleNamespace(extra={})
        live.voice_mention_gate_enabled = lambda extra: bool(r.get("voice_gate_enabled"))
        event = {"space": {"id": r.get("space_id"), "type": r.get("space_type")},
                 "content": r.get("content") or {}}
        chat, transcribes = a._gate_stt_chat(event)
        return {"chat": chat, "transcribes": transcribes}
    if f == "require_mention_flag":
        ENV.pop("PHOTON_REQUIRE_MENTION", None)
        if r.get("env") is not None:
            ENV["PHOTON_REQUIRE_MENTION"] = r["env"]
        extra = {}
        if r.get("value") is not None:
            extra["require_mention"] = r["value"]
        require_mention = extra.get("require_mention")
        if require_mention is None:
            require_mention = live._get_scoped_secret("PHOTON_REQUIRE_MENTION")
        return str(require_mention).strip().lower() in {"true", "1", "yes", "on"}
    if f == "markdown_enabled":
        ENV.pop("PHOTON_MARKDOWN", None)
        if r.get("value") is not None:
            ENV["PHOTON_MARKDOWN"] = r["value"]
        return live._markdown_enabled()
    if f == "reactions_enabled":
        ENV.pop("PHOTON_REACTIONS", None)
        if r.get("value") is not None:
            ENV["PHOTON_REACTIONS"] = r["value"]
        return _bare()._reactions_enabled()
    if f == "autostart_enabled":
        ENV.pop("PHOTON_SIDECAR_AUTOSTART", None)
        if r.get("value") is not None:
            ENV["PHOTON_SIDECAR_AUTOSTART"] = r["value"]
        autostart = str(live._get_scoped_secret("PHOTON_SIDECAR_AUTOSTART", "true")).lower()
        return autostart not in ("0", "false", "no")
    if f == "is_recent_richlink_preview":
        now = float(r.get("now") or 0.0)
        last = r.get("last")
        a = _bare()
        key = ADAPTER._normalize_chat_key(r.get("chat_id") or "")
        if last is not None:
            a._recent_richlinks_by_chat[key] = float(last)
        live.time = types.SimpleNamespace(time=lambda: now, monotonic=lambda: now)
        try:
            hit = a._is_recent_richlink_preview(r.get("chat_id") or "",
                                                r.get("content") or {})
        finally:
            live.time = REAL_TIME
        return {"hit": hit, "popped": last is not None and key not in a._recent_richlinks_by_chat}
    if f == "record_recent_richlink":
        now = float(r.get("now") or 0.0)
        a = _bare()
        live.time = types.SimpleNamespace(time=lambda: now, monotonic=lambda: now)
        try:
            a._record_recent_richlink(r.get("chat_id") or "", r.get("text") or "")
        finally:
            live.time = REAL_TIME
        key = ADAPTER._normalize_chat_key(r.get("chat_id") or "")
        return {"recorded": key in a._recent_richlinks_by_chat,
                "key": key if key in a._recent_richlinks_by_chat else None}
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
