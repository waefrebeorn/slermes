#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/adapter.py (gate w27_adapter_a96ad6_p13, slice: batching delay, extensions, dm-topic config).

Drives the LIVE upstream module by file path, so every comparison is against
the real implementation and not against a copy.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_a96ad6_p13"
NEEDS_SDK = False
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


def install_telegram_stub():
    """Provide the optional python-telegram-bot symbols this slice touches.

    The gate compares pure logic, not the SDK; where PTB is not installed the
    module-level import falls back to ``Any`` stubs, which is enough for every
    function here except the ones that *construct* SDK objects (BotCommand).
    """
    import types
    if "telegram" in sys.modules:
        return

    class _Any:
        def __init__(self, *a, **k):
            self.args, self.kwargs = a, k

    class BotCommand:
        def __init__(self, command, description=""):
            self.command, self.description = command, description

    tg = types.ModuleType("telegram")
    tg.BotCommand = BotCommand
    for name in ("Update", "Bot", "Message", "InlineKeyboardButton", "InlineKeyboardMarkup", "LinkPreviewOptions"):
        setattr(tg, name, _Any)
    ext = types.ModuleType("telegram.ext")
    for name in ("Application", "CommandHandler", "CallbackQueryHandler", "InlineQueryHandler",
                 "MessageHandler", "TypeHandler"):
        setattr(ext, name, _Any)
    ext.ContextTypes = type("ContextTypes", (), {"DEFAULT_TYPE": object})
    ext.filters = _Any
    const = types.ModuleType("telegram.constants")
    const.ParseMode = _Any
    const.ChatType = _Any
    req = types.ModuleType("telegram.request")
    req.HTTPXRequest = _Any
    tg.ext, tg.constants, tg.request = ext, const, req
    sys.modules["telegram"] = tg
    sys.modules["telegram.ext"] = ext
    sys.modules["telegram.constants"] = const
    sys.modules["telegram.request"] = req


if NEEDS_SDK:
    install_telegram_stub()
# The import chain resolves the Hermes root and the OS home; pin both so the
# oracle works under a bare bash (no USERPROFILE) as well as a login shell.
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(QUARRY))
_spec = importlib.util.spec_from_file_location(
    "tg_adapter_truth_" + GATE, str(QUARRY / "plugins/platforms/telegram/adapter.py"))
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)
A = live.TelegramAdapter


def bare(**attrs):
    """An adapter with no client/socket: only the given attributes exist."""
    a = A.__new__(A)
    for k, v in attrs.items():
        setattr(a, k, v)
    return a


def ns(x):
    if isinstance(x, dict):
        return NS(**{k: ns(v) for k, v in x.items()})
    if isinstance(x, list):
        return [ns(v) for v in x]
    return x


def mkexc(desc):
    """Build an exception whose class name (and bases) match desc."""
    import builtins
    desc = desc or {}
    name = desc.get("name") or "Exception"
    base = getattr(builtins, name, None)
    if not (isinstance(base, type) and issubclass(base, BaseException)):
        base = None
        try:
            from telegram import error as _terr
            cand = getattr(_terr, name, None)
            if isinstance(cand, type) and issubclass(cand, BaseException):
                base = cand
        except Exception:
            base = None
    if base is None:
        bases = (OSError,) if desc.get("os_error") else (Exception,)
        exc = type(name, bases, {})(desc.get("text") or "")
    else:
        exc = base(desc.get("text") or "")
    for attr in ("retry_after", "status_code", "code", "error_code"):
        if attr in desc:
            try:
                setattr(exc, attr, desc[attr])
            except Exception:
                pass
    return exc


class env_scope:
    """os.environ overlay for one case (the env rung of extra_or_secret)."""

    def __init__(self, env):
        self.env = env or {}

    def __enter__(self):
        self.saved = {}
        for k, v in self.env.items():
            self.saved[k] = os.environ.get(k)
            os.environ[k] = str(v)
        return self

    def __exit__(self, *a):
        for k, v in self.saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
        return False


def expected(r):
    f = r.get("func")
    if f == "text_batch_delay_for":
        a = bare(
            _SPLIT_THRESHOLD=r.get("split_threshold", 4000),
            _TEXT_BATCH_FAST_LEN=r.get("fast_len", 320),
            _TEXT_BATCH_FAST_DELAY_S=r.get("fast_delay", 0.18),
            _TEXT_BATCH_SHORT_LEN=r.get("short_len", 1024),
            _TEXT_BATCH_SHORT_DELAY_S=r.get("short_delay", 0.24),
            _text_batch_delay_seconds=r.get("delay", 1.5),
            _text_batch_split_delay_seconds=r.get("split_delay", 2.0),
        )
        pending = ns(r.get("pending")) if r.get("pending") else None
        return a._text_batch_delay_for(pending)
    if f == "ext_from_path":
        return A._ext_from_path(r.get("file_path"), list(r.get("candidates") or []), r.get("default") or "")
    if f == "get_dm_topic_info":
        a = bare(_dm_topics=dict(r.get("dm_topics") or {}),
                 _dm_topics_config=list(r.get("dm_topics_config") or []))
        a._reload_dm_topics_from_config = lambda: None
        return a._get_dm_topic_info(r.get("chat_id"), r.get("thread_id"))
    if f == "resolve_topic_binding":
        a = bare(_dm_topics=dict(r.get("dm_topics") or {}),
                 _dm_topics_config=list(r.get("dm_topics_config") or []))
        a._reload_dm_topics_from_config = lambda: None
        a._cache_dm_topic_from_message = lambda *args, **kwargs: None
        a.config = NS(extra={"group_topics": r.get("group_topics")})
        return list(a._resolve_topic_binding(ns(r.get("message")), r.get("chat_type"), r.get("thread_id")))
    if f == "group_observe_channel_prompt":
        a = bare(_bot=NS(id=r.get("bot_id")))
        if r.get("username"):
            a._bot_username_observed = r["username"]
        return a._telegram_group_observe_channel_prompt()
    raise AssertionError("unknown func %r" % (f,))



WANTED = {"text_batch_delay_for", "ext_from_path", "get_dm_topic_info", "resolve_topic_binding", "group_observe_channel_prompt"}


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
        if isinstance(py, (set, frozenset)):
            py = sorted(py)
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
