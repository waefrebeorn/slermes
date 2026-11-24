#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_media_flood.py (gate w19_telegramme_82fdeb).

Extracts the live module functions from the Python AST and drives them: `flood_wait_seconds`
against synthetic Telegram errors, `_KnownFloodWindow` for its text, and
`TelegramMediaFloodMixin._media_flood_recover` as a coroutine against a fake adapter, so the
recovery branch lattice is the LIVE one. The SendResult deferred marker and the recorded
cooldowns/sleeps are normalised to {"action","cooldown","sleep"} - the shape the C port emits.
"""
import ast
import asyncio
import json
import re
import sys
import types
import __future__
from contextvars import ContextVar
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
SRC = (REPO.parent / "hermes-agent" / "plugins" / "platforms" / "telegram"
       / "telegram_media_flood.py")
GATE = "w19_telegramme_82fdeb"
_ANN = __future__.annotations.compiler_flag


class _Logger:
    def warning(self, *a, **k):
        pass

    def error(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def exception(self, *a, **k):
        pass


class _AsyncStub:
    """asyncio with a recording sleep; everything else unused by the driven coroutine."""

    def __init__(self):
        self.sleeps = []

    def sleep(self, delay, *a, **k):
        self.sleeps.append(delay)

        async def _done():
            return None
        return _done()


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


G = {"__name__": "flood_live", "re": re, "logger": _Logger(), "ContextVar": ContextVar,
     "int": int}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            names = set()
            for t in targets:
                if isinstance(t, ast.Name):
                    names.add(t.id)
            if names & {"_FLOOD_TEXT_RE", "_MAX_WINDOW_WAITS", "_FLOOD_STAGE"}:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), G)
        if isinstance(node, ast.ClassDef) and node.name == "_KnownFloodWindow":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), G)
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in {
                "flood_wait_seconds", "_media_flood_recover"}:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), G)


extract()


class FloodError(Exception):
    def __init__(self, retry_after=None, text=None):
        super().__init__("flood control" if text is None else text)
        if retry_after is not None:
            self.retry_after = retry_after


class _Timedelta:
    def __init__(self, seconds):
        self._seconds = seconds

    def total_seconds(self):
        return self._seconds


class _Fake:
    name = "tg"

    def __init__(self):
        self.cooldowns = []

    def _record_send_flood_cooldown(self, chat_id, wait):
        self.cooldowns.append([chat_id, wait])
        return "REFUSED"

    async def _defer_media_upload(self, redo, source, wait, refused, what, target):
        return "DEFERRED"


async def _redo(source):
    return "REDO"


def _wait(**r):
    ra = r.get("retry_after")
    if isinstance(ra, dict) and "total_seconds" in ra:
        ra = _Timedelta(ra["total_seconds"])
    return G["flood_wait_seconds"](FloodError(retry_after=ra, text=r.get("text")))


def _recover(r):
    cap = r.get("inline_cap", 5.0)
    stub_module("plugins.platforms.telegram.adapter",
                _FLOOD_INLINE_WAIT_CAP_SECS=cap, _flood_cap_result=lambda w: {"cap": w})
    stage = int(r.get("stage", 0))
    token = G["_FLOOD_STAGE"].set(stage)
    try:
        if r.get("known_window"):
            err = G["_KnownFloodWindow"](float(r.get("wait") or 0.0))
        elif r.get("wait_present"):
            err = FloodError(retry_after=float(r.get("wait") or 0.0))
        else:
            err = FloodError(text=str(r.get("text") or "no flood here"))
        fake = _Fake()
        asyncio_stub = _AsyncStub()
        prev = G.get("asyncio")
        G["asyncio"] = asyncio_stub
        try:
            res = asyncio.run(G["_media_flood_recover"](fake, err, "chat", _redo,
                                                        "document f.pdf", None, None))
        finally:
            G["asyncio"] = prev
    finally:
        G["_FLOOD_STAGE"].reset(token)
    if res == "DEFERRED":
        action = "defer"
    elif res == "REDO":
        action = "inline_retry"
    elif res is None:
        action = "sleep_none" if asyncio_stub.sleeps else "none"
    else:
        action = "?" + repr(res)
    return {"action": action, "cooldown": bool(fake.cooldowns),
            "sleep": asyncio_stub.sleeps[0] if asyncio_stub.sleeps else 0}


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
        f = r.get("func")
        cases += 1
        try:
            if f == "flood_wait_seconds":
                py = _wait(**r)
            elif f == "known_window_message":
                py = str(G["_KnownFloodWindow"](float(r.get("wait") or 0.0)))
            elif f == "recover_action":
                py = _recover(r)
            else:
                mism += 1
                print("MISMATCH unknown func %r" % (f,), file=sys.stderr)
                continue
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s args=%r py=raised %r c=%r" % (f, r, exc, r.get("out")),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
