#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter_media.py (gate w19_adaptermed_c40d0e).

The live module body is re-executed from its own AST with the discord/aiohttp/i18n collaborators
stubbed; only the pure media helpers are exercised (Ogg/Opus duration, attachment cap,
oversized-upload preflight, reply chunk count).
"""
import ast
import asyncio
import base64
import contextlib
import json
import logging
import sys
import types
import __future__

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/plugins/platforms/discord/adapter_media.py"
GATE = "w19_adaptermed_c40d0e"
_ANN = __future__.annotations.compiler_flag

LOG = logging.getLogger("media_oracle")
LOG.addHandler(logging.NullHandler())
LOG.propagate = False
LOG.setLevel(logging.CRITICAL)
CUR = {"size": 0, "notify": False, "forum": False, "caption": None}


class SendResult:
    def __init__(self, success=False, error=None, message_id=None):
        self.success = success
        self.error = error
        self.message_id = message_id


class FakePath:
    @staticmethod
    def getsize(_p):
        return CUR["size"]

    @staticmethod
    def basename(p):
        return str(p).replace("\\", "/").rsplit("/", 1)[-1]

    @staticmethod
    def isfile(_p):
        return True

    @staticmethod
    def splitext(p):
        base = str(p).rsplit(".", 1)
        return (base[0], "." + base[1]) if len(base) == 2 else (str(p), "")


FAKE_OS = types.SimpleNamespace(path=FakePath(), unlink=lambda p: None, path_exists=lambda p: True)


def _t(key, **kw):
    return key


class FakeChannel:
    def __init__(self, forum=False):
        self.sent = []
        self.guild = None

    async def send(self, content=None, **_kw):
        self.sent.append(content)
        return types.SimpleNamespace(id=1, attachments=[])


def extract():
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    g = {"__name__": "media_live", "asyncio": asyncio, "base64": base64,
         "contextlib": contextlib, "json": json, "logging": logging, "os": FAKE_OS,
         "Path": __import__("pathlib").Path, "logger": LOG, "t": _t,
         "SendResult": SendResult, "Any": object, "Dict": dict, "List": list,
         "Optional": object, "Tuple": tuple, "__builtins__": __builtins__}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef, ast.Assign, ast.AnnAssign)):
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, SRC, "exec", flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
CLS = G["DiscordMediaMixin"]


def fake_self():
    return types.SimpleNamespace(
        name="discord",
        warning_notifications_enabled=lambda: CUR["notify"],
        _is_forum_parent=lambda _ch: CUR["forum"],
        format_message=lambda c: c,
        _discord_upload_limit_bytes=CLS._discord_upload_limit_bytes,
    )


def run_case(r):
    f = r.get("func")
    if f == "ogg_duration":
        data = bytes.fromhex(r.get("hex") or "")
        return G["_ogg_opus_duration"](data)
    if f == "upload_limit":
        ch = FakeChannel()
        guild = r.get("guild")
        if guild is not None:
            ch.guild = types.SimpleNamespace(filesize_limit=guild.get("filesize_limit"))
        return CLS._discord_upload_limit_bytes(ch)
    if f == "oversized":
        CUR["size"] = r.get("size", 0)
        CUR["notify"] = bool(r.get("notify"))
        CUR["forum"] = bool(r.get("forum"))
        ch = FakeChannel(forum=CUR["forum"])
        limit = r.get("limit", 1)
        ch.guild = types.SimpleNamespace(filesize_limit=limit)
        self_ = fake_self()
        res = asyncio.run(CLS._reject_oversized_upload(
            self_, ch, "C:/tmp/" + str(r.get("filename", "f.bin")),
            r.get("filename", "f.bin"), caption=CUR["caption"]))
        return {"rejected": res is not None, "error": None if res is None else res.error}
    if f == "reply_count":
        max_len = r.get("max_len", 2000)
        self_ = fake_self()
        self_.MAX_MESSAGE_LENGTH = max_len
        self_.truncate_message = lambda text, n: [text[i:i + n] for i in range(0, len(text), n)] or [text]
        return {"count": CLS.reply_message_count(self_, r.get("content", ""))}
    return "UNKNOWN_FUNC"


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
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
