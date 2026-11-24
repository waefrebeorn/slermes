#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_split.py
(gate w22_telegramsp_8c7e95).

The module imports only `re` and `typing`, so `_strip_mdv2`, `quotable_text`
and `split_reply_anchor` run live. `split_reply_anchor` needs an adapter; the
fake one below supplies exactly the three things it touches
(`_anchor_multipart_replies`, `_metadata_thread_id`, and
`_is_private_dm_topic_send`) and counts the predicate calls, which is how the
port's short-circuit order is checked:

    out = {"reply_to": <reply_to or None>, "dm_checked": <bool>}

`send_reply_document` / `reply_message_count` need a live TDLib adapter and are
not ported.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "telegram" / "telegram_split.py"
GATE = "w22_telegramsp_8c7e95"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("telegram_split_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["telegram_split_live"] = live
_spec.loader.exec_module(live)

WANTED = {"strip_mdv2", "quotable", "anchor", "const"}


class FakeAdapter:
    def __init__(self, anchor, dm, has_attr):
        if has_attr:
            self._anchor_multipart_replies = anchor
        self._dm = dm
        self.calls = 0

    def _metadata_thread_id(self, metadata):
        return (metadata or {}).get("thread_id")

    def _is_private_dm_topic_send(self, chat_id, thread_id, metadata):
        self.calls += 1
        return self._dm


def anchor(anchor_flag, parts, dm_topic, reply_to):
    has_attr = anchor_flag is not None
    adapter = FakeAdapter(bool(anchor_flag), bool(dm_topic), has_attr)
    value = live.split_reply_anchor(adapter, "chat-1", reply_to, {"thread_id": 7},
                                    int(parts if parts is not None else 1))
    return {"reply_to": value, "dm_checked": adapter.calls > 0}


def expected(r):
    f = r.get("func")
    if f == "strip_mdv2":
        return live._strip_mdv2(r.get("text") or "")
    if f == "quotable":
        return live.quotable_text(r.get("text") or "", mdv2=bool(r.get("mdv2")))
    if f == "anchor":
        return anchor(r.get("anchor_multipart"), r.get("parts"), r.get("dm_topic"),
                      r.get("reply_to"))
    if f == "const":
        return live.REPLY_FILE_MAX_BYTES
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
