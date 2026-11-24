#!/usr/bin/env python3
"""Differential oracle for gateway/run_turn_final_reconcile.py
(gate w26_runturnfin_c00b96).

`_flood_wait`, `_edit_carries_whole` and `owns_reconcilable_preview` are called
live.  A platform adapter is a seam, so each case describes one with attributes
(``SPLITS_OVERSIZED_FINAL_EDIT``, ``MAX_MESSAGE_LENGTH``, a ``format_message``
callable, and -- for a BasePlatformAdapter -- the per-chat length and unit).  The
consumer carries the five uncertainty flags, its ``message_id``, its adapter and
an optional ``_clean_for_display``.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_runturnfin_c00b96"

# The import chain resolves the Hermes home at import time; the process this
# oracle runs under may have no HOME at all. Point it at the repo parent.
_BASE = str(REPO.parent)
os.environ["USERPROFILE"] = _BASE
os.environ["HOME"] = _BASE
os.environ.setdefault("LOCALAPPDATA", str(Path(_BASE) / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway import run_turn_final_reconcile as live  # noqa: E402
from gateway.platforms.base import (  # noqa: E402
    _SEND_RETRY_INLINE_WAIT_CAP_SECS,
    BasePlatformAdapter,
)

WANTED = {"flood_wait", "carries_whole", "owns_preview", "constants"}


class _Result:
    def __init__(self, error=None, retry_after=None):
        self.error = error
        self.retry_after = retry_after


def _amp(text):
    return text.replace("&", "&amp;")


def _len_fn(unit):
    if unit == "utf16":
        return lambda text: len(text.encode("utf-16-le")) // 2
    return len


class _PlainAdapter:
    """Not a BasePlatformAdapter: only MAX_MESSAGE_LENGTH and plain len() apply."""

    def __init__(self, spec):
        self.SPLITS_OVERSIZED_FINAL_EDIT = bool(spec.get("splits"))
        limit = spec.get("limit")
        if limit is not None:
            self.MAX_MESSAGE_LENGTH = limit
        if spec.get("fmt") == "amp":
            self.format_message = staticmethod(_amp)


class _BaseAdapter(BasePlatformAdapter):  # type: ignore[misc]
    """A BasePlatformAdapter: per-chat length limit and length unit."""

    def __init__(self, spec):
        self.SPLITS_OVERSIZED_FINAL_EDIT = bool(spec.get("splits"))
        self._limit = spec.get("limit")
        self._unit = spec.get("len_fn") or "len"
        if spec.get("fmt") == "amp":
            self.format_message = staticmethod(_amp)

    def max_message_length_for_chat(self, chat_id):
        return self._limit

    def message_len_fn_for_chat(self, chat_id):
        return _len_fn(self._unit)


# The abstract send/receive surface is a seam here: only the length accessors
# are ever consulted, so the fake is instantiable.
_BaseAdapter.__abstractmethods__ = frozenset()


def adapter_of(spec):
    if not isinstance(spec, dict):
        return None
    if spec.get("base"):
        return _BaseAdapter(spec)
    return _PlainAdapter(spec)


def flood_wait(req):
    return live._flood_wait(_Result(req.get("error"), req.get("retry_after")))


def carries_whole(req):
    adapter = adapter_of(req)
    return live._edit_carries_whole(adapter, "chat", req.get("content") or "")


class _Consumer:
    def __init__(self, spec):
        self._turn_split_delivery = bool(spec.get("turn_split_delivery"))
        self.message_id = spec.get("message_id")
        self.adapter = adapter_of(spec.get("adapter"))
        self._use_native_streaming = bool(spec.get("native_streaming"))
        self._delivery_ambiguous = bool(spec.get("delivery_ambiguous"))
        self.delivery_uncertain = bool(spec.get("delivery_uncertain"))
        self._preview_uncertain = bool(spec.get("preview_uncertain"))
        self._egress_declined = bool(spec.get("egress_declined"))
        self.metadata = None
        if spec.get("clean") == "strip":
            self._clean_for_display = staticmethod(lambda text: text.strip())


def owns_preview(req):
    consumer = req.get("consumer")
    if consumer is None:
        return live.owns_reconcilable_preview(None)
    return live.owns_reconcilable_preview(_Consumer(consumer), req.get("chat_id") or "",
                                          req.get("content"))


def constants():
    return {
        "flood_error_prefix": "flood_control:",
        "flood_retry_default_seconds": 60.0,
        "max_flood_waits": live._MAX_FLOOD_WAITS,
        "flood_wait_slack_secs": live._FLOOD_WAIT_SLACK_SECS,
        "send_retry_inline_wait_cap_secs": _SEND_RETRY_INLINE_WAIT_CAP_SECS,
        "default_max_message_length": 4096,
        "no_edit_marker": "__no_edit__",
    }


def expected(r):
    f = r.get("func")
    if f == "flood_wait":
        return flood_wait(r)
    if f == "carries_whole":
        return carries_whole(r)
    if f == "owns_preview":
        return owns_preview(r)
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
