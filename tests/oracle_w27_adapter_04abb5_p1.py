#!/usr/bin/env python3
"""Differential oracle for the Buzz adapter text / url / identity leaves
(gate w27_adapter_04abb5_p1), driven against the LIVE
plugins/platforms/buzz/adapter.py.

Ported symbols and their live entry points:

  escape_mention      _escape_unresolved_presentation_mention(content, error)
  safe_filename       _safe_attachment_filename(value)
  attachment_origin   _attachment_origin(value)
  effective_port      _effective_port(urlsplit(url))
  is_relay_media_url  _is_relay_media_url(url, relay_url)
  find_relay_media_refs  _find_relay_media_refs(text, relay_url)
  replace_media_refs  _replace_media_refs(text, replacements)
  hex_to_npub         hex_to_npub(hex)
  constants           the module/class tuning values

No socket, subprocess, file or CLI is touched: every compared value is a pure
function of the JSON inputs the harness also feeds the C port.
"""
import json
import os
import sys
import tempfile
from pathlib import Path
from urllib.parse import urlsplit

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_04abb5_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The quarry's import chain resolves the Hermes home at import time; give it a
# throwaway one so nothing here can read (or write) the operator's real state.
_TMPHOME = tempfile.mkdtemp(prefix="w27-buzz-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

from plugins.platforms.buzz import adapter as live  # noqa: E402

WANTED = {
    "escape_mention", "safe_filename", "attachment_origin", "effective_port",
    "is_relay_media_url", "find_relay_media_refs", "replace_media_refs",
    "hex_to_npub", "constants",
}

CONSTANTS = (
    ("chat_kind", live, "_CHAT_KIND"),
    ("fetch_limit", live, "_FETCH_LIMIT"),
    ("dm_discovery_every", live, "_DM_DISCOVERY_EVERY"),
    ("default_poll_interval", live, "_DEFAULT_POLL_INTERVAL"),
    ("min_poll_interval", live, "_MIN_POLL_INTERVAL"),
    ("cli_timeout", live, "_CLI_TIMEOUT"),
    ("member_cache_ttl", live, "_MEMBER_CACHE_TTL"),
    ("profile_name_ttl", live, "_PROFILE_NAME_TTL"),
    ("max_inbound_attachments", live, "_MAX_INBOUND_ATTACHMENTS"),
    ("max_inbound_attachment_bytes", live, "_MAX_INBOUND_ATTACHMENT_BYTES"),
    ("attachment_download_timeout", live, "_ATTACHMENT_DOWNLOAD_TIMEOUT"),
    ("max_attachment_filename_bytes", live, "_MAX_ATTACHMENT_FILENAME_BYTES"),
    ("ws_auth_timeout", live, "_WS_AUTH_TIMEOUT"),
    ("ws_read_idle_timeout", live, "_WS_READ_IDLE_TIMEOUT"),
    ("ws_max_message_bytes", live, "_WS_MAX_MESSAGE_BYTES"),
    ("ws_membership_kind", live, "_WS_MEMBERSHIP_KIND"),
    ("event_meta_content_cap", live, "_EVENT_META_CONTENT_CAP"),
    ("thread_root_cache", live.BuzzAdapter, "_THREAD_ROOT_CACHE"),
    ("max_cli_message_chars", live, "_MAX_CLI_MESSAGE_CHARS"),
    ("ws_membership_sub_id", live, "_WS_MEMBERSHIP_SUB_ID"),
    ("presentation_mention_separator", live, "_BUZZ_PRESENTATION_MENTION_SEPARATOR"),
)


def norm(value):
    """Tuples compare unequal to JSON lists; normalise the live side."""
    if isinstance(value, tuple):
        return [norm(v) for v in value]
    if isinstance(value, list):
        return [norm(v) for v in value]
    if isinstance(value, dict):
        return {k: norm(v) for k, v in value.items()}
    return value


def constants():
    out = {key: getattr(owner, attr) for key, owner, attr in CONSTANTS}
    out["dispatch_kinds"] = sorted(live._DISPATCH_KINDS)
    try:
        from plugins.platforms.buzz import buzz_cursors as cursors
        out["seen_cap"] = cursors._SEEN_CAP
    except Exception:  # noqa: BLE001
        out["seen_cap"] = None
    return out


def expected(r):
    f = r.get("func")
    if f == "escape_mention":
        return live._escape_unresolved_presentation_mention(r.get("content") or "", r.get("error") or "")
    if f == "safe_filename":
        return live._safe_attachment_filename(r.get("value"))
    if f == "attachment_origin":
        return norm(live._attachment_origin(r.get("value")))
    if f == "effective_port":
        raw = r.get("url")
        try:
            parsed = urlsplit(str(raw or ""))
        except ValueError:
            return None
        return live._effective_port(parsed)
    if f == "is_relay_media_url":
        return live._is_relay_media_url(r.get("url") or "", r.get("relay_url") or "")
    if f == "find_relay_media_refs":
        urls, repls = live._find_relay_media_refs(r.get("text") or "", r.get("relay_url") or "")
        return {"urls": list(urls), "replacements": [list(x) for x in repls]}
    if f == "replace_media_refs":
        raw = r.get("replacements") or []
        repls = [(int(e[0]), int(e[1]), e[2]) for e in raw]
        return live._replace_media_refs(r.get("text") or "", repls)
    if f == "hex_to_npub":
        return live.hex_to_npub(r.get("hex") or "")
    if f == "constants":
        return constants()
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
