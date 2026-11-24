#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_tools_plugin_commands.py
(gate w24_methodstoo_3c78d0).

The live module is imported as a package module.  Its bodies reference the
globals server.py installs with ``method_ctx.bind_module``; here the oracle
installs *fakes* for exactly those seams, then calls the live functions:

  * ``_run_plugin_command`` -- the HERMES_SESSION_* key/cwd and the context
    dict handed to ``invoke_plugin_command`` are read back through the fakes
    (``_set_session_context`` records key/cwd, the fake invoke records kwargs);
  * ``_plugin_command_handler`` -- unbound, the live lookup is the error seam
    and answers None;
  * ``_dispatch_plugin_in_profile_scope`` -- the run / None / re-raise ladder,
    with a fake home scope, a runtime scope that can fail to enter, and a fake
    registry-command predicate.

No plugin is imported and no command runs: the handler call is the fake, and
the value it returns is the same string the C side composes.
"""
import contextlib
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_methodstoo_3c78d0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import methods_tools_plugin_commands as live  # noqa: E402

WANTED = {"run_command", "handler_lookup", "dispatch"}


class _Scope:
    def __init__(self, fails):
        self.fails = fails

    def __enter__(self):
        if self.fails:
            raise RuntimeError("bind failed")
        return self

    def __exit__(self, *exc):
        return False


def _install(r, seen):
    """Bind the server.py seams to deterministic fakes and record what the live
    code passes through them."""
    platform = r.get("platform", "")
    handler_present = bool(r.get("handler_present", True))

    def _invoke(handler, arg, **context):
        seen["context"] = dict(context)
        seen["call"] = (handler, arg)
        return "run|%s|%s|%s|%s|%s|%s" % (
            handler, arg, context.get("session_key"), context.get("session_id"),
            context.get("platform"), context.get("source"))

    def _set_context(key, cwd=""):
        seen["key"] = key
        seen["cwd"] = cwd
        return ["token"]

    plugins = types.SimpleNamespace(
        get_plugin_command_handler=lambda name: name if handler_present else None,
        plugin_command_absent_nowait=lambda name: bool(r.get("absent")),
        resolve_plugin_command_result=lambda value: value,
    )
    dispatch_mod = types.SimpleNamespace(invoke_plugin_command=_invoke)
    choices = types.SimpleNamespace(
        bind_asker=lambda asker: contextlib.nullcontext(),
        clarify_asker=lambda fn: fn,
    )
    mods = {
        "hermes_cli.plugins": plugins,
        "hermes_cli.plugin_command_dispatch": dispatch_mod,
        "hermes_cli.plugin_choices": choices,
    }
    live._tools_mod = lambda dotted: mods[dotted]
    live._set_session_context = _set_context
    live._clear_session_context = lambda tokens: None
    live._session_source = lambda session: platform
    live._clarify_block = lambda sid, questions: None
    live._session_home_scope = lambda session, profile=None: contextlib.nullcontext()
    live._profile_home = lambda profile=None: None
    live._session_profile_runtime_scope = lambda session: _Scope(bool(r.get("bind_fails")))
    live._is_registry_command = lambda name: bool(r.get("registry"))


def run_command(r):
    seen = {}
    _install(r, seen)
    session = r.get("session")
    result = live._run_plugin_command(r.get("handler", ""), r.get("arg", ""),
                                      session, r.get("sid", ""))
    return {"key": seen.get("key", ""), "cwd": seen.get("cwd", ""),
            "context": seen.get("context"), "result": result}


def handler_lookup(r):
    # Unbound module: the live lookup hits NameError and its except turns that
    # into None -- exactly the seam the port reports as null.
    live.__dict__.pop("_tools_mod", None)
    return live._plugin_command_handler(r.get("name", ""))


def dispatch(r):
    seen = {}
    _install(r, seen)
    try:
        result = live._dispatch_plugin_in_profile_scope(
            r.get("name", ""), r.get("arg", ""), r.get("session"),
            r.get("sid", ""), r.get("profile"))
    except Exception:  # noqa: BLE001 - the re-raise arm of the ladder
        return {"action": "raise", "result": None}
    if result is None:
        return {"action": "none", "result": None}
    return {"action": "run", "result": result}


def expected(r):
    f = r.get("func")
    try:
        if f == "run_command":
            return run_command(r)
        if f == "handler_lookup":
            return handler_lookup(r)
        if f == "dispatch":
            return dispatch(r)
    except Exception:  # noqa: BLE001 - the error seam: C must answer null
        return None
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
