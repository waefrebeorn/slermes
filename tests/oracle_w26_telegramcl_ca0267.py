#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_clarify_multi.py
(gate w26_telegramcl_ca0267).

The live coroutines are run for real (``asyncio.run``) with only their client
seams replaced: the Telegram button/i18n bindings, the callback answer, the
pending-clarify registry, the logger and the adapter's own edit/notice
methods.  What is compared is what those seams observed -- the keyboard rows,
the toggle/send decision, the labels a Send submits and the acknowledgement
body -- so the C port must reproduce behaviour, not plumbing.
"""
import asyncio
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_telegramcl_ca0267"

# The adapter import chain resolves the platform home directory; give it one.
import os  # noqa: E402
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.telegram import telegram_clarify_multi as live  # noqa: E402
import plugins.platforms.telegram.adapter as tg_adapter  # noqa: E402
import plugins.platforms.telegram.callback_answer as tg_answer  # noqa: E402
import plugins.platforms.telegram.telegram_clarify_binding as tg_binding  # noqa: E402
import tools.clarify_gateway as clarify_gateway  # noqa: E402

WANTED = {"keyboard", "pending_multi", "token_action", "resolve_ack"}

# Kept so a later case still exercises the real coroutine after an earlier one
# stubbed it (the stubs are module globals by design).
_ORIG_TOGGLE = live._toggle
_ORIG_RESOLVE = live.resolve_clarify_choice

# The t() seam: the two prompt labels the card renders.
LABELS = {
    "platform.telegram.prompt.other": "Other",
    "platform.telegram.prompt.send": "Send",
}


class _Entry:
    def __init__(self, raw):
        self.multi_select = bool(raw.get("multi_select"))
        self.choices = list(raw.get("choices") or [])


class _Button:
    def __init__(self, text, callback_data=None):
        self.text = text
        self.callback_data = callback_data


class _Markup:
    def __init__(self, rows):
        self.rows = rows


class _Query:
    def __init__(self, text=None):
        self.message = type("_Msg", (), {"text": text})()


class _Adapter:
    def __init__(self):
        self._clarify_state = {}
        self._clarify_multi = {}
        self.expired = False
        self.edited_html = None

    async def _notify_clarify_expired(self, query, user_display):
        self.expired = True

    async def _edit_html_quiet(self, query, text):
        self.edited_html = text


def keyboard(req):
    tg_adapter.InlineKeyboardButton = _Button
    tg_adapter.InlineKeyboardMarkup = _Markup
    tg_adapter.t = lambda key: LABELS[key]
    markup = live.multi_keyboard(None, req.get("clarify_id"),
                                 int(req.get("count") or 0),
                                 set(req.get("selected") or []))
    return {"rows": [[{"text": b.text, "callback_data": b.callback_data} for b in row]
                     for row in markup.rows]}


def pending_multi(req):
    raw = req.get("entry")
    entry = _Entry(raw) if isinstance(raw, dict) else None
    clarify_gateway.get_pending = lambda cid: entry
    return live.pending_multi_entry(req.get("clarify_id")) is not None


def token_action(req):
    raw = req.get("entry")
    entry = _Entry(raw) if isinstance(raw, dict) else None
    clarify_gateway.get_pending = lambda cid: entry
    tg_adapter._toast = lambda key, **kw: key
    seen = {"answers": [], "toggle": None, "resolve": None}

    async def _answer(query, text, **kw):
        seen["answers"].append(text)

    async def _toggle(adapter, query, cid, ent, selected, idx, user_display):
        seen["toggle"] = {"idx": idx, "selected": sorted(selected)}

    async def _resolve(adapter, query, cid, prompt, resolved_text, user_display, shown=None):
        seen["resolve"] = {"resolved_text": resolved_text, "shown": shown}

    tg_answer.answer_callback = _answer
    live._toggle = _toggle
    live.resolve_clarify_choice = _resolve
    _ = _ORIG_TOGGLE

    adapter = _Adapter()
    cid = req.get("clarify_id")
    adapter._clarify_multi[cid] = set(req.get("selected") or [])
    handled = asyncio.run(live.handle_multi_token(adapter, _Query(), cid, req.get("token"),
                                                  None, "alice"))
    return {"handled": handled, "expired": adapter.expired,
            "answers": seen["answers"], "toggle": seen["toggle"],
            "resolve": seen["resolve"]}


def resolve_ack(req):
    seen = {"answer": None, "logged": None}
    cid = req.get("clarify_id")

    async def _answer(query, text, **kw):
        seen["answer"] = text

    tg_answer.answer_callback = _answer
    clarify_gateway.resolve_gateway_clarify = lambda c_id, text: bool(req.get("resolved"))

    def _log_resolved(query, c_id, prompt, shown, user_display):
        seen["logged"] = {"clarify_id": c_id, "shown": shown, "user": user_display}

    tg_binding.log_resolved = _log_resolved
    adapter = _Adapter()
    asyncio.run(_ORIG_RESOLVE(adapter, _Query(req.get("message_text")), cid, None,
                              req.get("resolved_text"), req.get("user"),
                              shown=req.get("shown")))
    logged = seen["logged"]
    if logged is not None:
        logged = dict(logged, resolved_text=req.get("resolved_text"))
    return {"answer": seen["answer"], "html": adapter.edited_html,
            "expired": adapter.expired, "logged": logged}


def expected(r):
    f = r.get("func")
    if f == "keyboard":
        return keyboard(r)
    if f == "pending_multi":
        return pending_multi(r)
    if f == "token_action":
        return token_action(r)
    if f == "resolve_ack":
        return resolve_ack(r)
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
