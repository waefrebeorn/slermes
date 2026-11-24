#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/irc/adapter.py, wire format
(gate w33_adapter_eaa5eb_p1).

The live adapter is imported BY FILE PATH (its package __init__ drags the whole
gateway stack); host-blocked native extensions are stubbed first.  Only pure
value functions are compared:

  * ``_parse_irc_message`` / ``_extract_nick`` on raw protocol lines,
  * ``IRCAdapter._strip_markdown`` and the ``IRCAdapter._split_message``
    pipeline (markdown -> paragraphs -> byte-budget chunks),
  * ``_chunk_paragraph`` / ``_split_lines`` / ``_privmsg_budget``,
  * ``_encode_line`` / ``_strip_irc_control_chars``,
  * the PING -> "PONG :<token>" reply, driven through ``_handle_line`` with
    ``_send_raw`` as the recording seam.

The socket, the TLS context and the clock never start.  ``_split_message``
takes ``max_message_length`` from a bare instance, which is the only state it
reads.
"""
import asyncio
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_adapter_eaa5eb_p1"

# The package import chain reaches hermes_constants, which needs a home.
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location(
    "irc_adapter_live_p1", str(QUARRY / "plugins" / "platforms" / "irc" / "adapter.py"))
live = importlib.util.module_from_spec(_spec)
sys.modules["irc_adapter_live_p1"] = live
_spec.loader.exec_module(live)

ADAPTER = live.IRCAdapter

WANTED = {
    "parse_message", "extract_nick", "strip_markdown", "chunk_paragraph",
    "split_lines", "privmsg_budget", "split_message", "encode_line",
    "sanitize_outbound", "ping_reply",
}


def bare():
    """An adapter with no socket: only class-level behaviour is consulted."""
    return ADAPTER.__new__(ADAPTER)


def parse_message(r):
    return live._parse_irc_message(r["raw"])


def extract_nick(r):
    return live._extract_nick(r["prefix"])


def strip_markdown(r):
    return ADAPTER._strip_markdown(r["text"])


def chunk_paragraph(r):
    return live._chunk_paragraph(r["paragraph"], int(r["limit"]))


def split_lines(r):
    return live._split_lines(r["paragraphs"], int(r["limit"]))


def privmsg_budget(r):
    return live._privmsg_budget(r["target"])


def split_message(r):
    a = bare()
    a.max_message_length = int(r.get("max_message_length", 450))
    return ADAPTER._split_message(a, r.get("content", ""), r.get("target", ""))


def encode_line(r):
    return live._encode_line(r["line"]).decode("utf-8")


def sanitize_outbound(r):
    return live._strip_irc_control_chars(r["text"])


def ping_reply(r):
    params = r.get("params") or []
    raw = ("PING :" + params[0]) if params else "PING"
    a = bare()
    a._current_nick = r.get("nick", "hermes-bot")
    a.nickname = a._current_nick
    sent = []

    async def go():
        async def fake_send(line):
            sent.append(line)
        a._send_raw = fake_send
        await ADAPTER._handle_line(a, raw)

    asyncio.run(go())
    return sent[0] if sent else None


HANDLERS = {
    "parse_message": parse_message,
    "extract_nick": extract_nick,
    "strip_markdown": strip_markdown,
    "chunk_paragraph": chunk_paragraph,
    "split_lines": split_lines,
    "privmsg_budget": privmsg_budget,
    "split_message": split_message,
    "encode_line": encode_line,
    "sanitize_outbound": sanitize_outbound,
    "ping_reply": ping_reply,
}


def expected(r):
    f = r.get("func")
    fn = HANDLERS.get(f)
    if fn is None:
        raise AssertionError("unknown func %r" % (f,))
    return fn(r)


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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or cases == 0 else 0)


if __name__ == "__main__":
    main()
