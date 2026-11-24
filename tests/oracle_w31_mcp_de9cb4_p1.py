#!/usr/bin/env python3
"""Differential oracle for tools/connectors/mcp.py (gate w31_mcp_de9cb4_p1).

The live module is imported whole -- it is the tool entry point for MCP, and its
own imports are cheap -- and every seam it reaches for is replaced, not run:

  * the catalog / configured-name seams (``hermes_cli.mcp_catalog``) become the
    fixture's lists, so no installed servers are read and no config is written;
  * ``hosted_names`` (the connector gateway's account list) becomes the
    fixture's set;
  * ``redact_sensitive_text`` becomes the identity, so the comparison is exactly
    what this module decides -- the bare-error mapping and the replacement of the
    approved credentials -- and not what the redactor's regexes then do (that is
    agent/redact.py's own port).

Compared here: name validation and its messages, the misrouted-to-connector
message, error-detail text, the bounded tool-error body, the card answer's env
coercion, catalog instructions, the cancel note and the Try-again refusals, and
the module's literal constants.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_mcp_de9cb4_p1"

sys.path.insert(0, str(REPO / "tests"))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import agent.redact as redact_mod  # noqa: E402
from tools.connectors import mcp as live  # noqa: E402
from tools.connectors import mcp_oauth as oauth_mod  # noqa: E402
import hermes_cli.mcp_catalog as catalog_mod  # noqa: E402

WANTED = {
    "validate_names", "misrouted_error", "bare_error_text", "detail",
    "tool_error_body", "answer_env", "catalog_instructions", "cancel_note",
    "retry_error", "constants",
}


class _Exc(Exception):
    """A stand-in whose ``__name__`` is the fixture's exception class name."""


def _swapped(patches):
    """Context manager: set module attributes, restore them on exit."""
    class _Ctx:
        def __enter__(self):
            self._old = [(owner, name, getattr(owner, name)) for owner, name, _ in patches]
            for owner, name, value in patches:
                setattr(owner, name, value)
            return self

        def __exit__(self, *exc):
            for owner, name, value in self._old:
                setattr(owner, name, value)
            return False

    return _Ctx()


def validate_names(req):
    action = req.get("action") or ""
    names = req.get("names")
    names = list(names) if isinstance(names, list) else []
    catalog = req.get("catalog") if req.get("catalog") is not None else []
    configured = req.get("configured") if req.get("configured") is not None else []
    hosted = req.get("hosted")
    catalog_error = req.get("catalog_error")
    configured_error = req.get("configured_error")

    def read_catalog():
        if catalog_error is not None:
            raise RuntimeError(str(catalog_error))
        return sorted(str(n) for n in catalog)

    def read_configured():
        if configured_error is not None:
            raise RuntimeError(str(configured_error))
        return sorted(str(n) for n in configured)

    with _swapped([
        (live, "_catalog_names", read_catalog),
        (live, "_configured_names", read_configured),
        (live, "hosted_names", lambda: None if hosted is None else {str(n) for n in hosted}),
    ]):
        return live.validate_mcp_names(action, names)


def misrouted_error(req):
    return live.misrouted_to_mcp_error(req.get("action") or "", req.get("name") or "")


def bare_error_text(req):
    """The class-name chain ``_detail`` walks before it redacts anything."""
    cls = req.get("exc_class")
    if cls is None:
        exc = req.get("exc_message")
    else:
        exc = type(str(cls), (_Exc,), {})(("" if req.get("exc_message") is None
                                          else str(req.get("exc_message"))))
    text = str(exc) or (live._BARE_ERRORS.get(exc.__class__.__name__, exc.__class__.__name__)
                        if isinstance(exc, BaseException) else "error")
    return live._BARE_ERRORS.get(text, text)


def detail(req):
    cls = req.get("exc_class")
    if cls is None:
        exc = req.get("exc_message")
    else:
        exc = type(str(cls), (_Exc,), {})(("" if req.get("exc_message") is None
                                          else str(req.get("exc_message"))))
    approved = req.get("approved_env")
    runner = types.SimpleNamespace(approved_env=dict(approved) if isinstance(approved, dict) else {},
                                   action=req.get("action") or "install")
    target = types.SimpleNamespace(name=str(req.get("target") or ""))
    with _swapped([(redact_mod, "redact_sensitive_text", lambda text, **kw: text)]):
        return live._detail(exc, runner, target)


def tool_error_body(req):
    message = req.get("message")
    return json.loads(live.tool_error("" if message is None else str(message)))


def answer_env(req):
    return live._answer_env(req)


def catalog_instructions(req):
    present = bool(req.get("present"))
    post = req.get("post_install")
    entry = types.SimpleNamespace(post_install=post) if present else None
    with _swapped([(catalog_mod, "get_entry", lambda name: entry)]):
        return live._catalog_instructions(str(req.get("name") or "srv"))


def cancel_note(req):
    """``_cancel_attempt``: no flow -> no note; a cancel that lost the race -> the note."""
    has_flow = bool(req.get("has_flow"))
    cancelled = bool(req.get("cancelled"))
    target = types.SimpleNamespace(name="srv")
    work = types.SimpleNamespace(attempt=types.SimpleNamespace(flow=("flow" if has_flow else None)))
    runner = types.SimpleNamespace(work={"srv": work})
    if not has_flow:
        runner = types.SimpleNamespace(work={})
    with _swapped([(oauth_mod, "cancel_attempt", lambda flow: cancelled)]):
        return live._cancel_attempt(runner, target)


def retry_error(req):
    """``retry``: the two refusals, before any target is looked at."""
    runner = object() if req.get("runner") else None
    operation = types.SimpleNamespace(op_id="op1", settled=bool(req.get("settled")),
                                      target=lambda name: None)
    with _swapped([(live, "_RUNNERS", {"op1": runner} if runner is not None else {})]):
        return live.retry(operation, ["srv"])


def constants(req):
    return {
        "prepare_wait_seconds": float(live.PREPARE_WAIT_SECONDS),
        "note": live.NOTE,
        "no_card_note": live.NO_CARD_NOTE,
        "authorization_kept": live.AUTHORIZATION_KEPT,
        "bare_errors": dict(live._BARE_ERRORS),
    }


def expected(r):
    f = r.get("func")
    if f == "validate_names":
        return validate_names(r)
    if f == "misrouted_error":
        return misrouted_error(r)
    if f == "bare_error_text":
        return bare_error_text(r)
    if f == "detail":
        return detail(r)
    if f == "tool_error_body":
        return tool_error_body(r)
    if f == "answer_env":
        return answer_env(r)
    if f == "catalog_instructions":
        return catalog_instructions(r)
    if f == "cancel_note":
        return cancel_note(r)
    if f == "retry_error":
        return retry_error(r)
    if f == "constants":
        return constants(r)
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
