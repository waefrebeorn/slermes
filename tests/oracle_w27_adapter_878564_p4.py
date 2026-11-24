#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/photon/adapter.py (gate w27_adapter_878564_p4).

Gate 4 compares IDS/KEYS, sidecar error typing, retry classification, the module
constants and timestamp shaping.  Same by-path import + seam stubs as gate 1.

Seams fed from the fixture:
  * `base_retryable` → ENV["BASE_RETRYABLE"], the base adapter's verdict that
    PhotonAdapter._is_retryable_error short-circuits on.
  * `now` → a `datetime` subclass whose `.now()` is fixed, so the live
    `_parse_timestamp` fallback is deterministic.

`PhotonSidecarError` is returned as the live object and projected to its field
dict + str(e); integral floats compare equal to ints.
"""
import importlib.util
import json
import sys
import types
from datetime import UTC, datetime
from enum import Enum
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_878564_p4"

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
            return bool(ENV.get("BASE_RETRYABLE"))

        @staticmethod
        def validate_media_delivery_path(p):
            return str(p) if p else None

    base.BasePlatformAdapter = BasePlatformAdapter
    base.SendResult = SendResult
    sys.modules["gateway.platforms.base"] = base

    shared = types.ModuleType("gateway.platforms._shared")
    shared.coerce_port = lambda v, d: d
    shared.extra_or_secret = lambda extra, k, e, d="", **kw: d
    shared.get_scoped_secret = lambda k, d=None, **kw: d
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

WANTED = {"normalize_chat_key", "sent_text_key", "is_dm_guid",
          "sidecar_error_from_response", "retryable_error",
          "is_permanent_sidecar_failure", "send_retry_is_final", "constants",
          "parse_timestamp"}

CONSTANTS = {
    "default_sidecar_port": lambda: live._DEFAULT_SIDECAR_PORT,
    "default_sidecar_bind": lambda: live._DEFAULT_SIDECAR_BIND,
    "max_message_length": lambda: live._MAX_MESSAGE_LENGTH,
    "dedup_max_size": lambda: live._DEDUP_MAX_SIZE,
    "dedup_window_seconds": lambda: live._DEDUP_WINDOW_SECONDS,
    "fffc_wait_seconds": lambda: live._FFFC_WAIT_SECONDS,
    "npm_reinstall_timeout": lambda: live._NPM_REINSTALL_TIMEOUT,
    "richlink_preview_suppress_seconds": lambda: live._RICHLINK_PREVIEW_SUPPRESS_SECONDS,
    "typing_cooldown_seconds": lambda: live._TYPING_COOLDOWN_SECONDS,
    "runtime_record_name": lambda: live._RUNTIME_RECORD_NAME,
    "richlink_preview_attachment_suffix": lambda: live._RICHLINK_PREVIEW_ATTACHMENT_SUFFIX,
    "target_not_allowed_message": lambda: live._TARGET_NOT_ALLOWED_MESSAGE,
    "retryable_patterns": lambda: list(live._PHOTON_RETRYABLE_PATTERNS),
    "default_mention_patterns": lambda: list(live._DEFAULT_MENTION_PATTERNS),
}

REAL_DATETIME = datetime


def _bare():
    return ADAPTER.__new__(ADAPTER)


def expected(r):
    f = r.get("func")
    if f == "normalize_chat_key":
        return ADAPTER._normalize_chat_key(r.get("chat_id") or "")
    if f == "sent_text_key":
        return live._sent_text_key(r.get("chat_id"))
    if f == "is_dm_guid":
        return bool(live._SEND_DM_GUID_RE.fullmatch(r.get("text") or ""))
    if f == "sidecar_error_from_response":
        e = live._sidecar_error_from_response(
            r.get("path") or "", int(r.get("status_code") or 0),
            r.get("text") or "", r.get("data"))
        return {"path": e.path, "status_code": e.status_code, "error": e.error,
                "error_class": e.error_class, "retryable": bool(e.retryable),
                "message": str(e)}
    if f == "retryable_error":
        ENV["BASE_RETRYABLE"] = bool(r.get("base_retryable"))
        try:
            return ADAPTER._is_retryable_error(r.get("error") or "")
        finally:
            ENV["BASE_RETRYABLE"] = False
    if f == "is_permanent_sidecar_failure":
        return ADAPTER._is_permanent_sidecar_failure(
            SendResult(success=False, raw_response=r.get("result", {}).get("raw_response")))
    if f == "send_retry_is_final":
        return _bare()._send_retry_is_final(
            SendResult(success=False, raw_response=r.get("result", {}).get("raw_response")))
    if f == "constants":
        return {k: fn() for k, fn in CONSTANTS.items()}
    if f == "parse_timestamp":
        fixed = REAL_DATETIME.fromisoformat(r.get("now") or "2026-11-24T05:00:00+00:00")
        if fixed.tzinfo is None:
            fixed = fixed.replace(tzinfo=UTC)

        class FakeDateTime(REAL_DATETIME):
            @classmethod
            def now(cls, tz=None):
                return fixed

        live.datetime = FakeDateTime
        try:
            ts = r.get("ts")
            out = live._parse_timestamp(ts or "")
            try:
                probe = REAL_DATETIME.fromisoformat(ts) if ts else None
            except ValueError:
                probe = None
            return {"iso": out.isoformat(), "used_now": probe is None}
        finally:
            live.datetime = REAL_DATETIME
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
