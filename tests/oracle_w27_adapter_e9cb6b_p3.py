#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/adapter.py (gate p3).

Every predicate/mutator here is driven on a bare ``MatrixAdapter`` made with
``__new__`` (no client, no connect, no __init__ side effects): what is compared
is exactly what those methods decide from their arguments.  Two shaping rules
that live inside coroutines (the approval tier pick and the model-picker
flattening) are evaluated with the same expression the surrounding coroutine
uses, since the coroutine itself awaits a client.
"""

import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_e9cb6b_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("HERMES_HOME", tempfile.mkdtemp(prefix="w27-oracle-home-"))

from plugins.platforms.matrix import adapter as live  # noqa: E402

ADAPTER = live.MatrixAdapter

WANTED = {
    "exec_approval_tier", "approval_reaction", "approval_action", "flat_picker_choices",
    "choice_picker_emoji", "apply_relation_metadata", "protect_mention_regions",
    "extract_outbound_mentions", "has_outbound_room_mention",
    "inject_outbound_mention_links", "is_bot_mentioned", "user_localpart",
    "strip_mention", "mxc_to_http", "sanitize_link_url", "format_message",
}

# adapter.py:751-753, the map __init__ installs on every instance.
APPROVAL_MAP = {
    "\u2705": "once", "\U0001f300": "session", "\u267e\ufe0f": "always", "\u267e": "always",
    "\u274c": "deny", "\u274e": "deny",
}


def bare(user_id="", homeserver="https://hs.example", allow_room=False):
    o = ADAPTER.__new__(ADAPTER)
    o._user_id = user_id
    o._homeserver = homeserver
    o._allow_room_mentions = allow_room
    return o


def _tier(r):
    choices = r.get("choices") or []
    tier = "once"
    if not r.get("smart_denied"):
        tier = "always" if "always" in choices else ("session" if "session" in choices else "once")
    return tier


def _reaction(r):
    return ADAPTER._EA_REACTIONS.get(r.get("tier"))


def _action(r):
    return APPROVAL_MAP.get(r.get("emoji"))


def _flat(r):
    providers = r.get("providers") or []
    flat = [
        (str(model_id), str(p.get("slug") or ""), str(p.get("name") or p.get("slug") or ""))
        for p in providers for model_id in (p.get("models") or [])
    ][:len(live._MATRIX_MODEL_PICKER_REACTIONS)]
    return [list(row) for row in flat]


def _choice_emoji(r):
    out = {}
    for emoji, choice in zip(live._MATRIX_CHOICE_PICKER_REACTIONS, r.get("choices") or []):
        out[emoji] = str(choice.get("value") or "")
    return out


def _relation(r):
    content = dict(r.get("content") or {})
    meta = {"thread_id": r.get("thread_id")} if r.get("thread_id") is not None else None
    ADAPTER._apply_relation_metadata(bare(), content, reply_to=r.get("reply_to"), metadata=meta)
    return content


def _protect(r):
    protected, placeholders = bare()._protect_outbound_mention_regions(r.get("text"))
    # The sentinel framing is a C-string constraint: Python masks with NUL, the
    # C port with \x01 (a C string cannot carry NUL).  Rewrite before comparing.
    for i in range(len(placeholders)):
        protected = protected.replace("\x00MENTION_PROTECTED%d\x00" % i,
                                      "\x01MENTION_PROTECTED%d\x01" % i)
    return {"protected": protected, "placeholders": list(placeholders)}


def _extract(r):
    return bare()._extract_outbound_mentions(r.get("text"))


def _has_room(r):
    return bare()._has_outbound_room_mention(r.get("text"))


def _inject(r):
    return bare()._inject_outbound_mention_links(r.get("text"))


def _bot_mentioned(r):
    return bare(r.get("user_id") or "")._is_bot_mentioned(
        r.get("body"), r.get("formatted_body"), r.get("mention_user_ids"))


def _localpart(r):
    return bare(r.get("user_id") or "")._user_localpart()


def _strip(r):
    return bare(r.get("user_id") or "")._strip_mention(r.get("body"))


def _mxc(r):
    return bare(homeserver=r.get("homeserver") or "")._mxc_to_http(r.get("mxc_url") or "")


def _sanitize(r):
    return ADAPTER._sanitize_link_url(r.get("url"))


def _format(r):
    return ADAPTER.format_message(bare(), r.get("content"))


CASES = {
    "exec_approval_tier": _tier,
    "approval_reaction": _reaction,
    "approval_action": _action,
    "flat_picker_choices": _flat,
    "choice_picker_emoji": _choice_emoji,
    "apply_relation_metadata": _relation,
    "protect_mention_regions": _protect,
    "extract_outbound_mentions": _extract,
    "has_outbound_room_mention": _has_room,
    "inject_outbound_mention_links": _inject,
    "is_bot_mentioned": _bot_mentioned,
    "user_localpart": _localpart,
    "strip_mention": _strip,
    "mxc_to_http": _mxc,
    "sanitize_link_url": _sanitize,
    "format_message": _format,
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
