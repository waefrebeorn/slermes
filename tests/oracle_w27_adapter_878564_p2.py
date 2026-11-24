#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/photon/adapter.py (gate w27_adapter_878564_p2).

Gate 2 compares the inbound content PREDICATES and LABELS.  The live module is
imported by file path with the same seam stubs as gate 1 (the real
`gateway.platforms.helpers` is loaded by path); every function compared here is a
module-level pure function of the payload dict, so no adapter instance, cache or
clock is involved.

MessageType members compare by their `.value` (the C port emits the value
string), and the normalize-style helpers are not exercised in this gate.
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
GATE = "w27_adapter_878564_p2"


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

WANTED = {"is_voice_payload", "cached_media_type", "attachment_message_type",
          "group_item_contents", "richlink_url_from_content",
          "is_richlink_preview_attachment", "richlink_preview_label",
          "is_richlink_preview_content", "reply_inner", "carries_voice",
          "mention_gate_text"}


def expected(r):
    f = r.get("func")
    payload = r.get("payload") or {}
    content = r.get("content") or {}
    if f == "is_voice_payload":
        return live._is_voice_payload(payload)
    if f == "cached_media_type":
        return live._cached_media_type(payload, r.get("mime") or "",
                                       bool(r.get("is_voice")))
    if f == "attachment_message_type":
        return live._attachment_message_type(r.get("mime") or "").value
    if f == "group_item_contents":
        return live._group_item_contents(content)
    if f == "richlink_url_from_content":
        return live._richlink_url_from_content(content)
    if f == "is_richlink_preview_attachment":
        return live._is_richlink_preview_attachment(payload)
    if f == "richlink_preview_label":
        return live._richlink_preview_label(content)
    if f == "is_richlink_preview_content":
        return live._is_richlink_preview_content(content)
    if f == "reply_inner":
        return live._reply_inner(content)
    if f == "carries_voice":
        return live._carries_voice(content)
    if f == "mention_gate_text":
        return live._mention_gate_text(content)
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
