#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/adapter.py (gate p4).

Inbound policy is driven on a bare ``MatrixAdapter`` (``__new__``) carrying only
the flags each predicate reads.  Two derivations that live inside
``_resolve_room_identity`` (the server-name slice and the identity record) are
evaluated with the exact expressions that method uses, because the surrounding
method awaits the state store and the clock.
"""

import json
import os
import sys
import tempfile
import time
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_e9cb6b_p4"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("HERMES_HOME", tempfile.mkdtemp(prefix="w27-oracle-home-"))

for _k in ("GATEWAY_ALLOW_ALL_USERS", "MATRIX_REQUIRE_MENTION"):
    os.environ.pop(_k, None)

from plugins.platforms.matrix import adapter as live  # noqa: E402

ADAPTER = live.MatrixAdapter

WANTED = {
    "is_self_sender", "is_system_or_bridge_sender", "classify_inbound_media",
    "voice_may_park", "content_mentions_bot", "extract_invite_dm_signal",
    "matrix_prompt_expired", "is_authorized_user", "reactor_allowed",
    "state_event_value", "room_server_name", "room_identity", "presence_valid",
}


def bare(user_id="", require_mention=True, free_rooms=(), allowed_rooms=(), threads=()):
    o = ADAPTER.__new__(ADAPTER)
    o._user_id = user_id
    o._require_mention = require_mention
    o._free_rooms = set(free_rooms)
    o._allowed_rooms = set(allowed_rooms)
    o._threads = set(threads)
    o._authorization_check = None
    o._allowed_user_ids = set()
    return o


def _self_sender(r):
    return bare(r.get("user_id") or "")._is_self_sender(r.get("sender") or "")


def _bridge(r):
    return ADAPTER._is_system_or_bridge_sender(r.get("sender") or "")


def _classify(r):
    mt, mime, voice = ADAPTER._classify_inbound_media(
        r.get("msgtype") or "", r.get("mimetype") or "", r.get("content") or {})
    return [mt.value, mime, voice]


def _voice_park(r):
    o = bare(r.get("user_id") or "",
             require_mention=bool(r.get("require_mention")),
             free_rooms=r.get("free_rooms") or (),
             allowed_rooms=r.get("allowed_rooms") or (),
             threads=r.get("bot_threads") or ())
    return o._voice_may_park(r.get("room_id") or "", r.get("body") or "",
                             r.get("content") or {}, r.get("relates_to") or {},
                             bool(r.get("mention_claimed")))


def _content_mentions(r):
    return bare(r.get("user_id") or "")._content_mentions_bot(
        r.get("body") or "", r.get("content") or {})


def _invite_signal(r):
    direct, inviter = bare(r.get("user_id") or "")._extract_invite_dm_signal(r.get("invited_room"))
    return [direct, inviter]


def _expired(r):
    prompt = type("P", (), {"expires_at": r.get("expires_at")})()
    orig = time.monotonic
    time.monotonic = lambda: r.get("now")
    try:
        return bare()._matrix_prompt_expired(prompt)
    finally:
        time.monotonic = orig


def _authorized(r):
    o = bare()
    o._allowed_user_ids = set(r.get("allowed_users") or [])
    # The env opt-in is the other half of _is_authorized_user; plant it.
    os.environ.pop("GATEWAY_ALLOW_ALL_USERS", None)
    if r.get("allow_all"):
        os.environ["GATEWAY_ALLOW_ALL_USERS"] = "true"
    try:
        return o._is_authorized_user(r.get("user_id") or "") is True
    finally:
        os.environ.pop("GATEWAY_ALLOW_ALL_USERS", None)


def _reactor(r):
    # The sender rule of _validate_matrix_prompt_reactor (adapter.py:2489-2492).
    require = True if r.get("require_sender") is None else bool(r.get("require_sender"))
    requester = r.get("requester")
    if require and requester and r.get("sender") != requester:
        return False
    return True


def _state_value(r):
    return ADAPTER._state_event_value(r.get("event"), r.get("key") or "")


def _server_name(r):
    room_id = r.get("room_id") or ""
    return (room_id.rsplit(":", 1)[-1].strip() or None) if ":" in room_id else None


def _identity(r):
    room_id = r.get("room_id") or ""
    room_name = r.get("room_name")
    canonical_alias = r.get("canonical_alias")
    member_count = r.get("member_count")
    is_direct = bool(r.get("is_direct"))
    has_explicit_name = bool(room_name)
    is_likely_dm = ((member_count is not None and member_count <= 2)
                    or (is_direct and not has_explicit_name))
    return {
        "server_name": (room_id.rsplit(":", 1)[-1].strip() or None) if ":" in room_id else None,
        "display_name": room_name or canonical_alias or room_id,
        "has_explicit_name": has_explicit_name,
        "chat_type": "dm" if is_likely_dm else "room",
        "conflict": bool(is_direct and has_explicit_name
                         and (member_count is None or member_count > 2)),
    }


def _presence(r):
    return r.get("state") in ADAPTER._VALID_PRESENCE_STATES


CASES = {
    "is_self_sender": _self_sender,
    "is_system_or_bridge_sender": _bridge,
    "classify_inbound_media": _classify,
    "voice_may_park": _voice_park,
    "content_mentions_bot": _content_mentions,
    "extract_invite_dm_signal": _invite_signal,
    "matrix_prompt_expired": _expired,
    "is_authorized_user": _authorized,
    "reactor_allowed": _reactor,
    "state_event_value": _state_value,
    "room_server_name": _server_name,
    "room_identity": _identity,
    "presence_valid": _presence,
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
            py = CASES[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
