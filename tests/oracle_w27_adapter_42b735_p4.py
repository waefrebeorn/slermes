#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/whatsapp/adapter.py slice 4
(gate w27_adapter_42b735_p4): outbound shaping + bridge-facing tables.

All pure: no instance state, no filesystem, no socket.

  * ``_bridge_media_type``            extension -> image|video|audio|document;
  * ``_normalize_outbound_mentions``  JID normalization + order-preserving
                                      dedupe (gateway.whatsapp_identity);
  * the module tables the bridge is configured from (one constants view).
"""
import contextlib
import importlib.util
import io
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_42b735_p4"

# The msys bash that runs this oracle has no USERPROFILE, and pathlib.Path.home()
# (touched at import time by hermes_constants) raises without it.  Give it the
# real Windows home the repo lives under.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(Path(REPO).parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = os.environ["USERPROFILE"]

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "wa_adapter_live_" + GATE, QUARRY / "plugins" / "platforms" / "whatsapp" / "adapter.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
with contextlib.redirect_stdout(io.StringIO()):
    _spec.loader.exec_module(live)

ADAPTER = live.WhatsAppAdapter
WANTED = {"bridge_media_type", "normalize_outbound_mentions", "constants"}


def as_list(value):
    return value if isinstance(value, list) else []


def bridge_media_type(req):
    return live._bridge_media_type(
        str(req.get("file_path") or ""),
        bool(req.get("is_voice")),
        bool(req.get("force_document")),
    )


def normalize_outbound_mentions(req):
    raw = req.get("mentions")
    mentions = [str(m) for m in raw] if isinstance(raw, list) else None
    return live._normalize_outbound_mentions(mentions)


def _enum(value):
    return getattr(value, "value", value)


def constants():
    ext_table = {}
    for ext in (".jpg", ".jpeg", ".png", ".webp", ".gif",
                ".mp4", ".mov", ".avi", ".mkv", ".webm", ".3gp",
                ".ogg", ".opus", ".mp3", ".wav", ".m4a", ".flac"):
        ext_table[ext] = live._WA_EXT_MEDIA_TYPE.get(ext, "document")
    return {
        "passkey_required_exit": live.PASSKEY_REQUIRED_EXIT,
        "owner_reply_prefix": live._OWNER_REPLY_PREFIX,
        "max_text_inject_bytes": live._MAX_TEXT_INJECT_BYTES,
        "split_threshold": ADAPTER._SPLIT_THRESHOLD,
        "text_inject_exts": sorted(live._TEXT_INJECT_EXTS),
        "bridge_passthrough_env": list(live._BRIDGE_PASSTHROUGH_ENV),
        "bridge_consumed_env": list(live._BRIDGE_CONSUMED_ENV),
        "native_media_types": {k: _enum(v) for k, v in live._NATIVE_MEDIA_TYPES.items()},
        "media_needles": [[needle, _enum(kind)] for needle, kind in live._MEDIA_NEEDLES],
        "media_info": {_enum(k): [v[0], v[1]] for k, v in live._MEDIA_INFO.items()},
        "media_type_by_bridge_kind": {k: _enum(v) for k, v in live._MEDIA_TYPE_BY_BRIDGE_KIND.items()},
        "quoted_mime_by_bridge_kind": dict(live._QUOTED_MIME_BY_BRIDGE_KIND),
        "wa_ext_media_type": ext_table,
        "yaml_bridge": [list(row) for row in live._YAML_BRIDGE],
    }


DISPATCH = {
    "bridge_media_type": bridge_media_type,
    "normalize_outbound_mentions": normalize_outbound_mentions,
    "constants": lambda req: constants(),
}


def expected(req):
    return DISPATCH[req["func"]](req)


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
            with contextlib.redirect_stdout(io.StringIO()):
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
