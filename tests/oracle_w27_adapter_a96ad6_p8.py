#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/adapter.py (gate w27_adapter_a96ad6_p8, slice: bot command menu + platform event envelopes).

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
GATE = "w27_adapter_a96ad6_p8"
NEEDS_SDK = True
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
    if f == "bot_commands":
        rows = live.TelegramAdapter._bot_commands([tuple(row) for row in r.get("menu_commands") or []])
        return [[c.command, c.description] for c in rows]
    if f == "is_id_like":
        return A._is_id_like(r.get("value"))
    if f == "normalize_platform_event":
        return bare()._normalize_platform_event(ns(r.get("update")))
    if f == "normalize_reaction_event":
        return bare()._normalize_reaction_event(ns(r.get("update")))
    if f == "normalize_message_edited_event":
        upd = ns(r.get("update"))
        msg = getattr(upd, "edited_message", None)
        if msg is not None and isinstance(getattr(msg, "edit_date", None), str):
            from datetime import datetime
            msg.edit_date = datetime.fromisoformat(msg.edit_date)
        return bare()._normalize_message_edited_event(upd)
    raise AssertionError("unknown func %r" % (f,))



WANTED = {"bot_commands", "is_id_like", "normalize_platform_event", "normalize_reaction_event", "normalize_message_edited_event"}


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
