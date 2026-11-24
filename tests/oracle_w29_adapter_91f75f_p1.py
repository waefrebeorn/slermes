#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/dingtalk/adapter.py, gate p1.

The live Stream Mode adapter is imported from the quarry tree and its value
core is called directly.  The socket/event-loop layer is the seam: no
websocket is opened, no SDK is installed (the module's optional imports are
already guarded), no task is created.  What is compared is exactly what the
inbound path decides with:

  * ``_csv_set`` / ``_decode_json_list_literal`` over list, CSV, JSON-list
    string, bool, int and null settings;
  * ``_normalize_markdown`` (the renderer workarounds);
  * ``_dingtalk_require_mention`` over the resolved setting;
  * ``_is_user_allowed`` over the lowered allowlist;
  * ``_should_process_message`` (fake compiled patterns supply the regex
    hits; the group gate composition is the live method's);
  * ``_is_sdk_incompat`` over real exception objects wired with
    ``__cause__``/``__context__``;
  * ``get_chat_info``;
  * the module budget constants.

An adapter instance is made with ``__new__`` and given only what these
methods touch, so no PlatformConfig, no deduplicator and no http client ever
exists.
"""
import asyncio
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_91f75f_p1"

# A login shell can start without a home; the package resolves one at import.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(QUARRY.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.dingtalk import adapter as live  # noqa: E402

ADAPTER = live.DingTalkAdapter

WANTED = {"constants", "csv_set", "normalize_markdown", "require_mention",
          "is_user_allowed", "should_process_message", "is_sdk_incompat",
          "chat_info"}

# The env rung of extra_or_secret: cleared so the fixture's config.extra is the
# only rung that can answer (this is the documented single-profile behaviour).
ENV_KEYS = ("DINGTALK_ALLOWED_CHATS", "DINGTALK_FREE_RESPONSE_CHATS",
            "DINGTALK_REQUIRE_MENTION", "DINGTALK_ALLOWED_USERS",
            "DINGTALK_CLIENT_ID", "DINGTALK_CLIENT_SECRET")
for _k in ENV_KEYS:
    os.environ.pop(_k, None)


class _Cfg:
    """The two attributes the group gate reads off PlatformConfig."""

    def __init__(self, extra):
        self.extra = extra if isinstance(extra, dict) else {}

    platform = live.Platform.DINGTALK


class _Pattern:
    """Stand-in for a compiled wake-word regex: .search() reports the hit."""

    def __init__(self, hit):
        self.hit = bool(hit)

    def search(self, text):
        return object() if self.hit else None


def bare(extra=None):
    a = ADAPTER.__new__(ADAPTER)
    a.config = _Cfg(extra)
    a.platform = live.Platform.DINGTALK
    a._mention_patterns = []
    a._allowed_users = set()
    a._session_webhooks = {}
    return a


def _setting(raw):
    """extra_or_secret(..., default "", blank_is_unset=False) given a cleared env."""
    return "" if raw is None else raw


def csv_set(r):
    return sorted(live._csv_set(r.get("raw")))


def normalize_markdown(r):
    return ADAPTER._normalize_markdown(r.get("text"))


def require_mention(r):
    a = bare({"require_mention": r.get("configured")})
    return a._dingtalk_require_mention()


def is_user_allowed(r):
    a = bare()
    a._allowed_users = {i.lower() for i in live._csv_set(_setting(r.get("allowed_users")))}
    return a._is_user_allowed(r.get("sender_id"), r.get("sender_staff_id"))


def should_process_message(r):
    m = r.get("message")
    m = m if isinstance(m, dict) else {}
    a = bare({
        "allowed_chats": m.get("allowed_chats"),
        "free_response_chats": m.get("free_response_chats"),
        "require_mention": m.get("require_mention"),
    })
    hits = m.get("pattern_hits")
    a._mention_patterns = [_Pattern(h) for h in hits] if isinstance(hits, list) else []
    msg = type("_Msg", (), {})()
    msg.is_in_at_list = bool(m.get("is_in_at_list"))
    return a._should_process_message(msg, m.get("text") or "",
                                     bool(m.get("is_group")), m.get("chat_id") or "")


class _SubTypeError(TypeError):
    """A TypeError subclass: isinstance() still holds, the class name differs."""


def _exc_class(spec):
    name = spec.get("type")
    if spec.get("te"):
        return _SubTypeError
    if name == "TypeError":
        return TypeError
    if name == "AttributeError":
        return AttributeError
    if name == "RuntimeError":
        return RuntimeError
    return type(str(name or "Exception"), (Exception,), {})


def build_chain(chain):
    """Materialise the fixture's abstract exception chain into real objects."""
    if not isinstance(chain, list) or not chain:
        return None
    nodes = []
    for spec in chain:
        spec = spec if isinstance(spec, dict) else {}
        nodes.append(_exc_class(spec)(spec.get("msg") or ""))
    for i, spec in enumerate(chain):
        spec = spec if isinstance(spec, dict) else {}
        cause, context = spec.get("cause"), spec.get("context")
        nodes[i].__cause__ = nodes[cause] if isinstance(cause, int) else None
        nodes[i].__context__ = nodes[context] if isinstance(context, int) else None
    return nodes[0]


def is_sdk_incompat(r):
    return live._is_sdk_incompat(build_chain(r.get("chain")))


def chat_info(r):
    return asyncio.run(ADAPTER.get_chat_info(bare(), r.get("chat_id")))


def constants(_r):
    return {
        "max_message_length": live.MAX_MESSAGE_LENGTH,
        "reconnect_backoff": list(live.RECONNECT_BACKOFF),
        "reconnect_circuit_breaker_trips": live.RECONNECT_CIRCUIT_BREAKER_TRIPS,
        "reconnect_circuit_breaker_delay": live.RECONNECT_CIRCUIT_BREAKER_DELAY,
        "sdk_log_repeat_window": float(live._SDK_LOG_REPEAT_WINDOW),
        "session_webhooks_max": live._SESSION_WEBHOOKS_MAX,
        "emotion_id": live._EMOTION_ID,
        "emotion_bg": live._EMOTION_BG,
        "truthy": sorted(live._TRUTHY),
        "numbered_re": live._NUMBERED_RE.pattern,
        "webhook_re": live._DINGTALK_WEBHOOK_RE.pattern,
        "no_local_upload": live._NO_LOCAL_UPLOAD,
    }


CALLS = {
    "constants": constants,
    "csv_set": csv_set,
    "normalize_markdown": normalize_markdown,
    "require_mention": require_mention,
    "is_user_allowed": is_user_allowed,
    "should_process_message": should_process_message,
    "is_sdk_incompat": is_sdk_incompat,
    "chat_info": chat_info,
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
            py = CALLS[r["func"]](r)
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
