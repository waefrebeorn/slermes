#!/usr/bin/env python3
"""Differential oracle for tools/computer_use/cua_backend_session.py (gate
w23_cuabackend_f09034).

The module is the cua-driver MCP session: an asyncio bridge thread, a lazily
started stdio ``ClientSession`` and a ``cua-driver call`` subprocess fallback.
All three are process seams, so the live functions are run against scripted
transport facts:

  * an exception instance is rebuilt from the fixture's class ``__name__`` /
    ``__module__`` / base (the ``isinstance`` seam);
  * ``_CuaDriverSession`` instances are made with ``__new__`` so the
    ``tools/list`` capability and schema maps are exactly the fixture's;
  * MCP model objects are ``SimpleNamespace``s -- attribute presence is what
    ``_mcp_field``/``_tool_field`` read.

Everything that is not a seam -- the fail-closed envelope text and shape, the
logical-error flattening (``json.dumps(..., sort_keys=True)``), the ended-session
recognition, both error classifiers and the capability lookups -- is live.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tools" / "computer_use" / "cua_backend_session.py"
GATE = "w23_cuabackend_f09034"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("cua_backend_session_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["cua_backend_session_live"] = live
_spec.loader.exec_module(live)

WANTED = {
    "outcome_unknown", "logical_error_text", "is_ended_session_result",
    "is_closed_session_error", "is_transient_daemon_error", "supports_capability",
    "supports_input_property", "mcp_field", "tool_field", "cli_attempts",
}

_BASES = {"broken_pipe": BrokenPipeError, "eof": EOFError}


def pseudo_exc(r):
    """An exception whose class name/module/base are the fixture's."""
    base = _BASES.get(r.get("base"), Exception)
    cls = type(r.get("name") or "PseudoError", (base,),
               {"__module__": r.get("module") or "__main__"})
    return cls("x")


def session(r):
    """A session carrying exactly the fixture's tools/list maps (no transport)."""
    obj = live._CuaDriverSession.__new__(live._CuaDriverSession)
    obj._capabilities = {k: set(v) for k, v in (r.get("capabilities") or {}).items()}
    obj._tool_schemas = {k: v for k, v in (r.get("schemas") or {}).items()}
    return obj


def model(r):
    return types.SimpleNamespace(**(r.get("obj") or {}))


def expected(r):
    f = r.get("func")
    if f == "cli_attempts":
        return live._CLI_ATTEMPTS
    if f == "outcome_unknown":
        return live._outcome_unknown(r.get("name"), RuntimeError(r.get("detail", "")),
                                     r.get("code"))
    if f == "logical_error_text":
        return live._logical_error_text(r.get("error") or {})
    if f == "is_ended_session_result":
        return live._is_ended_session_result(r.get("result") or {})
    if f == "is_closed_session_error":
        return live._CuaDriverSession._is_closed_session_error(pseudo_exc(r))
    if f == "is_transient_daemon_error":
        return live._CuaDriverSession._is_transient_daemon_error(Exception(r.get("message") or ""))
    if f == "supports_capability":
        return session(r).supports_capability(r.get("capability"), r.get("tool"))
    if f == "supports_input_property":
        return session(r).supports_input_property(r.get("tool"), r.get("property"))
    if f == "mcp_field":
        return live._mcp_field(model(r), r.get("snake"), r.get("camel"))
    if f == "tool_field":
        return live._tool_field(model(r), r.get("snake"), r.get("camel"))
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
