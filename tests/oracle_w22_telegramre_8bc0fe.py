#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_resolved_prompt.py
(gate w22_telegramre_8bc0fe).

The live coroutine `edit_keeping_prompt` is lifted out of the source with an AST
walk and executed against a fake Telegram surface: `telegram.error` provides a
real `BadRequest`/`NetworkError` pair, the query records every
`edit_message_text` call, and the adapter records `_edit_md_quiet`. Nothing of
the function's own logic is re-implemented here -- the expected value is the
trace of what the live coroutine did.

Per case:
  escape  -- live `html.escape(text)`.
  strip   -- live `body.replace(deadline, "")`.
  plan    -- run the coroutine and report the path it took, the text of the
             edit it attempted (parse_mode included), and the decision handed
             to the quiet fallback.
"""
import ast
import asyncio
import html
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "telegram" / "telegram_resolved_prompt.py"
GATE = "w22_telegramre_8bc0fe"

WANTED = {"escape", "strip", "plan"}


class _Log:
    def debug(self, *a, **k):
        pass


# --- the live coroutine, with only telegram.error supplied -------------------
class BadRequest(Exception):
    pass


class NetworkError(Exception):
    pass


_pkg = types.ModuleType("telegram")
_err = types.ModuleType("telegram.error")
_err.BadRequest = BadRequest
_err.NetworkError = NetworkError
_pkg.error = _err
sys.modules["telegram"] = _pkg
sys.modules["telegram.error"] = _err

G = {"html": html, "logger": _Log(), "Any": object}
_tree = ast.parse(SRC.read_text(encoding="utf-8"))
for _node in _tree.body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name == "edit_keeping_prompt":
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)
        break
else:  # pragma: no cover
    raise SystemExit("edit_keeping_prompt not found in %s" % SRC)
edit_keeping_prompt = G["edit_keeping_prompt"]


# --- the transport seam -----------------------------------------------------
class FakeMessage:
    def __init__(self, text_html):
        self.text_html = text_html


class FakeQuery:
    def __init__(self, body, error):
        self.message = FakeMessage(body)
        self.error = error
        self.edits = []

    async def edit_message_text(self, text, parse_mode=None, reply_markup=None):
        self.edits.append({"text": text, "parse_mode": parse_mode,
                           "reply_markup": reply_markup})
        if self.error == "BadRequest":
            raise BadRequest("Message is too long")
        if self.error == "NetworkError":
            raise NetworkError("timed out")
        if self.error:
            raise RuntimeError("unexpected")


class FakeAdapter:
    def __init__(self, deadline):
        self.deadline = deadline
        self.quiet = []

    def _ea_deadline_line(self):
        return self.deadline

    async def _edit_md_quiet(self, query, decision):
        self.quiet.append(decision)


def plan(body, deadline, decision, error, message=True):
    query = FakeQuery(body if message else None, error)
    adapter = FakeAdapter(deadline)
    raised = None
    try:
        asyncio.run(edit_keeping_prompt(adapter, query, decision))
    except Exception as exc:  # noqa: BLE001 - an unexpected raise is the answer
        raised = exc
    if raised is not None:
        path = "raise"
    elif query.edits and adapter.quiet:
        path = "quiet_after_badrequest"
    elif query.edits and error == "NetworkError":
        path = "return_after_network"
    elif query.edits:
        path = "edit"
    else:
        path = "quiet_no_body"
    return {
        "path": path,
        "text": query.edits[0]["text"] if query.edits else None,
        "parse_mode": query.edits[0]["parse_mode"] if query.edits else None,
        "quiet_decision": adapter.quiet[0] if adapter.quiet else None,
    }


def expected(r):
    f = r.get("func")
    if f == "escape":
        return html.escape(r.get("text") or "")
    if f == "strip":
        return (r.get("body") or "").replace(r.get("deadline") or "", "")
    if f == "plan":
        return plan(r.get("body"), r.get("deadline") or "", r.get("decision") or "",
                    r.get("error") or "", bool(r.get("message", True)))
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
