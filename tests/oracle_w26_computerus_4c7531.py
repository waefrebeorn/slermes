#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/computer_use.py
(gate w26_computerus_4c7531).

The live handlers are called for real -- ``_exit_if_plugin_backend`` and
``_cu_perms_status`` -- with only their process seams replaced: the plugin
backend registry (``plugins.computer_use``), the TCC probe module
(``tools.computer_use.permissions``) and stdout/sys.exit.  Printed lines are
captured verbatim, so the C port has to reproduce every line and exit code.
The argparse tree is derived from the real ``build_computer_use_parser`` and
compared as the literal table the port mirrors.
"""
import argparse
import io
import json
import sys
from contextlib import redirect_stdout
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_computerus_4c7531"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli.subcommands import computer_use as live  # noqa: E402
import plugins.computer_use as plugin_backends  # noqa: E402
import tools.computer_use.permissions as permissions  # noqa: E402

WANTED = {"backend_gate", "perms_status", "surface"}


class _Args:
    def __init__(self, **kw):
        self.__dict__.update(kw)


class _Provider:
    def __init__(self, req):
        self.name = req.get("name") or ""
        self.display_name = req.get("display_name") or ""
        self._rc = req.get("provider_rc")
        self._ok = bool(req.get("available"))

    def doctor(self):
        return self._rc

    def is_available(self):
        return self._ok


def _capture(fn):
    code = None
    buf = io.StringIO()
    try:
        with redirect_stdout(buf):
            fn()
    except SystemExit as exc:
        code = exc.code
    return {"exit": code is not None, "code": code,
            "lines": buf.getvalue().splitlines()}


def backend_gate(req):
    plugin_backends.DEFAULT_BACKEND = req.get("default_backend")
    plugin_backends.configured_backend_name = lambda: req.get("configured_backend")
    err = req.get("lookup_error")
    if err is not None:
        def _raise():
            raise LookupError(err)
        plugin_backends.get_active_provider = _raise
    else:
        provider = _Provider(req)
        plugin_backends.get_active_provider = lambda: provider
    return _capture(lambda: live._exit_if_plugin_backend(doctor=bool(req.get("doctor"))))


def perms_status(req):
    st = req.get("st")
    st = st if isinstance(st, dict) else {}
    permissions.computer_use_status = lambda: st
    permissions.TCC_FIELDS = tuple(req.get("tcc_fields") or ("accessibility", "screen_recording"))
    permissions.stale_tcc_grant_hint = lambda *fields: req.get("hint")
    plugin_backends.DEFAULT_BACKEND = ""
    plugin_backends.configured_backend_name = lambda: ""
    return _capture(lambda: live._cu_perms_status(_Args(json=False)))


def _opt(a):
    default = a.default
    if default is argparse.SUPPRESS:
        default = None
    choices = getattr(a, "choices", None)
    return {
        "flags": list(a.option_strings),
        "dest": a.dest,
        "action": type(a).__name__,
        "nargs": a.nargs,
        "default": default,
        "choices": sorted(choices) if isinstance(choices, (list, tuple)) else None,
        "metavar": getattr(a, "metavar", None),
    }


def _helps(parser):
    out = {}
    for a in parser._actions:
        if isinstance(a, argparse._SubParsersAction):
            for pseudo in a._choices_actions:
                out[pseudo.dest] = pseudo.help
    return out


def _node(parser, name, help_text):
    node = {"name": name, "help": help_text, "options": [], "subcommands": []}
    helps = _helps(parser)
    for a in parser._actions:
        if isinstance(a, argparse._SubParsersAction):
            for sub_name, sub in a.choices.items():
                node["subcommands"].append(_node(sub, sub_name, helps.get(sub_name)))
        elif isinstance(a, argparse._HelpAction):
            continue
        else:
            node["options"].append(_opt(a))
    return node


def surface(req):
    top = argparse.ArgumentParser(prog="hermes")
    subs = top.add_subparsers(dest="cmd")
    live.build_computer_use_parser(subs)
    parser = subs.choices["computer-use"]
    return _node(parser, "computer-use",
                 "Manage the Computer Use (cua-driver) backend (macOS/Windows/Linux)")


def expected(r):
    f = r.get("func")
    if f == "backend_gate":
        return backend_gate(r)
    if f == "perms_status":
        return perms_status(r)
    if f == "surface":
        return surface(r)
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
