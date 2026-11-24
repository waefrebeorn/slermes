#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter_vault_code.py (gate w19_adaptervau_0f2138).

The card is rendered by the live `send_vault_code_prompt` coroutine with the i18n facade
fed from the fixture's templates, so the compared text is exactly what the module builds.
"""
import ast
import asyncio
import dataclasses
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
SRC = Q / "plugins" / "platforms" / "slack" / "adapter_vault_code.py"
GATE = "w19_adaptervau_0f2138"
_ANN = __future__.annotations.compiler_flag


_FN = (ast.FunctionDef, ast.AsyncFunctionDef)


def _exec_nodes(path, funcs, assigns, classes, g):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in tree.body:
        keep = ((isinstance(node, _FN) and node.name in funcs)
                or (isinstance(node, ast.ClassDef) and node.name in classes)
                or (isinstance(node, ast.Assign) and any(
                    isinstance(t, ast.Name) and t.id in assigns for t in node.targets)))
        if keep:
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, str(path), "exec", flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name == "SlackVaultCodeMixin":
            for sub in node.body:
                if isinstance(sub, _FN) and sub.name in funcs:
                    mod = ast.Module(body=[sub], type_ignores=[])
                    exec(compile(mod, str(path), "exec", flags=_ANN, dont_inherit=True), g)
    return g


TEMPLATES = {}


def _t(key, **kwargs):
    tmpl = TEMPLATES.get(key, "")
    try:
        return tmpl.format(**kwargs)
    except Exception:  # noqa: BLE001 - a template without the placeholder renders verbatim
        return tmpl


# A real module object: @dataclass resolves its class's module through sys.modules.
_live_mod = types.ModuleType("av_live")
sys.modules["av_live"] = _live_mod
G = _live_mod.__dict__
G.update({"__name__": "av_live", "asyncio": asyncio, "secrets": __import__("secrets"),
          "dataclass": dataclasses.dataclass, "time": __import__("time"),
          "logging": __import__("logging"), "t": _t, "Any": object, "Callable": object,
          "Optional": object, "SendResult": object})
_exec_nodes(SRC, {"_mrkdwn_escape", "send_vault_code_prompt", "_live_vault_code_card",
            "_vault_code_author_matches"},
            {"VIEW_CALLBACK", "OPEN_ACTION", "_BLOCK_ID", "_INPUT_ACTION", "_HINT_LIMIT",
             "_MODAL_TITLE_LIMIT", "_REQUESTS_MAX"}, {"_CodeCard", "_DropVaultCodeFrames"}, G)

# the facade `t` the module reaches through plugins.platforms.slack.adapter
for _name in ("plugins", "plugins.platforms", "plugins.platforms.slack",
              "plugins.platforms.slack.adapter"):
    if _name not in sys.modules:
        _m = types.ModuleType(_name)
        _m.__path__ = []
        sys.modules[_name] = _m
sys.modules["plugins.platforms.slack.adapter"].t = _t


def case_mrkdwn_escape(r):
    text = r.get("text")
    return G["_mrkdwn_escape"]("" if text is None else str(text))


def case_author_matches(r):
    card = SimpleNamespace(user_id=r.get("card_user") or "", team_id=r.get("card_team") or "")
    return G["_vault_code_author_matches"](None, card, r.get("user_id") or "", r.get("team_id") or "")


def case_card_text(r):
    TEMPLATES.clear()
    TEMPLATES.update({
        "platform.shared.vault_code.body": r.get("body_template", ""),
        "platform.shared.vault_code.title": r.get("title_template", ""),
        "platform.shared.vault_code.button": r.get("button_template", ""),
        "platform.shared.vault_code.input_label": "Code",
        "platform.shared.vault_code.ended": "Ended",
    })
    captured = {}

    class Self_:
        def _metadata_team_id(self, metadata):
            return r.get("team_id")

        def _button(self, text, action, token, style=None):
            captured["button"] = text
            return {"type": "button", "text": text, "action_id": action, "style": style}

        def _vault_code_cards(self):
            return {}

        def _trim_oldest_dict_entries(self, cards, limit):
            return None

        async def _send_interactive_prompt(self, chat_id, metadata, build, name, sanitize=True):
            title, blocks = build()
            captured["title"] = title
            captured["blocks"] = blocks
            return SimpleNamespace(success=True, message_id="1", raw_response={"channel": chat_id})

    asyncio.run(G["send_vault_code_prompt"](Self_(), "C1", r.get("site") or "", r.get("hint") or "",
                                            user_id="U1", submit=lambda code: True,
                                            timeout_s=30.0, metadata=None))
    blocks = captured["blocks"]
    return {"text": blocks[0]["text"]["text"], "title": captured["title"],
            "button": captured["button"], "blocks": blocks}


def case_live_card(r):
    card = SimpleNamespace(expires_at=r.get("expires_at"))
    holder = SimpleNamespace(_vault_code_cards=lambda: {"tok": card})
    G["time"] = SimpleNamespace(monotonic=lambda: r.get("now", 0.0))
    out = G["_live_vault_code_card"](holder, "tok")
    return out is not None


def case_filter_allows(r):
    proc = G["_DropVaultCodeFrames"]()
    record = SimpleNamespace(getMessage=lambda: r.get("message_text") or "")
    return bool(proc.filter(record))


CASES = {"mrkdwn_escape": case_mrkdwn_escape, "author_matches": case_author_matches,
         "card_text": case_card_text, "live_card": case_live_card,
         "filter_allows": case_filter_allows}


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
