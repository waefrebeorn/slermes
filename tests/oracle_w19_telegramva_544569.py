#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_vault_code.py (gate w19_telegramva_544569).

The card matcher, the pop (pending counter + bounded retired map) and the ForceReply prompt run
as the live methods against fixture-built state; `now`, the replies and the retired map are the
seams, so both sides see the same clock and the same card tables.
"""
import ast
import asyncio
import dataclasses
import html
import json
import sys
import types
import __future__
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
Q = REPO.parent / "hermes-agent"
SRC = Q / "plugins" / "platforms" / "telegram" / "telegram_vault_code.py"
GATE = "w19_telegramva_544569"
_ANN = __future__.annotations.compiler_flag
_FN = (ast.FunctionDef, ast.AsyncFunctionDef)

TEMPLATES = {}


def _t(key, **kwargs):
    tmpl = TEMPLATES.get(key, "")
    try:
        return tmpl.format(**kwargs)
    except Exception:  # noqa: BLE001
        return tmpl


def _exec_nodes(path, funcs, assigns, classes, g):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in tree.body:
        keep = ((isinstance(node, _FN) and node.name in funcs)
                or (isinstance(node, ast.ClassDef) and node.name in classes)
                or (isinstance(node, ast.Assign) and any(
                    isinstance(t, ast.Name) and t.id in assigns for t in node.targets)))
        if keep:
            _run(node, g, path)
        elif isinstance(node, ast.ClassDef) and node.name == "TelegramVaultCodeMixin":
            for sub in node.body:
                if isinstance(sub, _FN) and sub.name in funcs:
                    _run(sub, g, path)
    return g


def _run(node, g, path):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, str(path), "exec", flags=_ANN, dont_inherit=True), g)


_live_mod = types.ModuleType("tvc_live")
sys.modules["tvc_live"] = _live_mod
G = _live_mod.__dict__
G.update({"__name__": "tvc_live", "html": html, "asyncio": asyncio,
          "logging": __import__("logging"), "weakref": __import__("weakref"),
          "time": SimpleNamespace(monotonic=lambda: 0.0),
          "dataclass": dataclasses.dataclass, "_t": _t,
          "logger": SimpleNamespace(debug=lambda *a, **k: None),
          "Any": object, "Callable": object, "Optional": object, "SendResult": object})
_exec_nodes(SRC, {"_match_vault_code_card", "_pop_vault_code_reply", "send_vault_code_prompt"},
            {"_HINT_LIMIT", "_PLACEHOLDER_LIMIT", "_RETIRED_MAX"},
            {"_CodeReply", "_DropPendingCodeUpdates"}, G)

# --- lazily-imported collaborators ---
for _name in ("telegram", "plugins", "plugins.platforms", "plugins.platforms.telegram",
              "plugins.platforms.telegram.adapter"):
    if _name not in sys.modules:
        _m = types.ModuleType(_name)
        _m.__path__ = []
        sys.modules[_name] = _m


class ForceReply:
    def __init__(self, selective=False, input_field_placeholder=None):
        self.selective = selective
        self.input_field_placeholder = input_field_placeholder


sys.modules["telegram"].ForceReply = ForceReply
sys.modules["plugins.platforms.telegram.adapter"].ParseMode = SimpleNamespace(HTML="HTML")
sys.modules["plugins.platforms.telegram.adapter"].t = _t


def card_tables(r):
    replies = {}
    for e in r.get("replies") or []:
        replies[(str(e["chat_id"]), str(e["message_id"]))] = SimpleNamespace(
            user_id=str(e.get("user_id")), expires_at=e.get("expires_at", 0.0))
    retired = {}
    for e in r.get("retired") or []:
        k = e.get("key") or []
        retired[(str(k[0]), str(k[1]))] = e.get("user_id")
    return replies, retired


def case_match_card(r):
    replies, retired = card_tables(r)
    replied_to = r.get("replied_to")
    msg = SimpleNamespace(
        chat_id=r.get("chat_id"),
        from_user=None if r.get("user_id") is None else SimpleNamespace(id=r.get("user_id")),
        reply_to_message=None if replied_to is None else SimpleNamespace(message_id=replied_to),
        chat=SimpleNamespace(type=r.get("chat_type") or ""))
    G["time"] = SimpleNamespace(monotonic=lambda: r.get("now", 0.0))
    holder = SimpleNamespace(_vault_code_replies=lambda: replies,
                             _retired_vault_code_cards=lambda: retired)
    key, to_ended = G["_match_vault_code_card"](holder, msg)
    return {"key": list(key) if key is not None else None, "to_ended": bool(to_ended)}


def case_pop_reply(r):
    replies, retired = card_tables(r)
    cls = G["_DropPendingCodeUpdates"]
    cls.pending = int(r.get("pending", 0))
    holder = SimpleNamespace(_vault_code_replies=lambda: replies,
                             _retired_vault_code_cards=lambda: retired)
    reply = G["_pop_vault_code_reply"](holder, (str(r.get("chat_id")), str(r.get("message_id"))))
    return {
        "found": reply is not None,
        "pending": cls.pending,
        "replies": [{"chat_id": k[0], "message_id": k[1], "user_id": v.user_id,
                     "expires_at": v.expires_at} for k, v in replies.items()],
        "retired": [{"key": list(k), "user_id": v} for k, v in retired.items()],
    }


def case_prompt_text(r):
    TEMPLATES.clear()
    TEMPLATES.update({
        "platform.shared.vault_code.title": r.get("title_template", ""),
        "platform.telegram.vault_code.body": r.get("body_template", ""),
        "platform.shared.vault_code.input_label": r.get("label_template", ""),
    })
    replies = {}
    cap = {}

    class Self_:
        def _metadata_thread_id(self, metadata):
            return None

        def _vault_code_replies(self):
            return replies

        async def _send_prompt(self, name, chat_id, metadata, build, parse_mode=None, thread_id=None):
            text, markup, remember = build()
            cap["markup"] = markup
            remember(SimpleNamespace(message_id=701, chat_id=chat_id))
            return SimpleNamespace(success=True, markup=markup, text=text)

    asyncio.run(G["send_vault_code_prompt"](Self_(), str(r.get("chat_id")), r.get("site") or "",
                                            r.get("hint") or "", user_id=str(r.get("user_id")),
                                            submit=lambda code: True, timeout_s=30.0, metadata=None))
    key = next(iter(replies))
    return {"text": replies[key].prompt_text,
            "placeholder": cap["markup"].input_field_placeholder}


def case_filter_allows(r):
    cls = G["_DropPendingCodeUpdates"]
    cls.pending = int(r.get("pending", 0))
    record = SimpleNamespace(getMessage=lambda: r.get("message_text") or "")
    return bool(cls().filter(record))


CASES = {"match_card": case_match_card, "pop_reply": case_pop_reply,
         "prompt_text": case_prompt_text, "filter_allows": case_filter_allows}


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
        if f not in CASES:
            continue
        cases += 1
        try:
            py = json.loads(json.dumps(CASES[f](r), default=str))
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
