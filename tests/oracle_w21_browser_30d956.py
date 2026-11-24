#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/browser.py (gate w21_browser_30d956).

`build_browser_parser` is driven for real into an argparse parser, and the
dispatch closure it installs as the `browser` parser's `func` default is called
on each parsed argv. The OS/process seams are stubbed in sys.modules with
exact-behaviour fakes: `browser_connect` (default-browser detection, profile
directory, terminating the process tree) and `browser_preview_signins`
(`run_export_signins` returns the fixture's export_rc). argparse's own help text
is canonicalized to "<HELP>" -- it belongs to argparse, not to this module.
"""
import argparse
import contextlib
import importlib.util
import io
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "browser.py"
GATE = "w21_browser_30d956"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("subcommands_browser_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["subcommands_browser_live"] = live
_spec.loader.exec_module(live)

STATE = {}


def install_stubs():
    """OS/process seam: the three machine calls and the export runner."""
    conn = types.ModuleType("hermes_cli.browser_connect")
    conn.UNSUPPORTED_CHANNEL = "__unsupported_channel__"
    conn.detect_default_chromium = lambda *a, **k: STATE.get("detected")
    conn.real_profile_data_dir = lambda browser, *a, **k: (STATE.get("profile") or {}).get(browser)
    conn.close_browser_holding_profile = lambda src, *a, **k: (
        bool(STATE.get("closed")), STATE.get("msg") or "")
    sys.modules["hermes_cli.browser_connect"] = conn
    preview = types.ModuleType("hermes_cli.browser_preview_signins")
    preview.run_export_signins = lambda args: STATE.get("export_rc", 0)
    sys.modules["hermes_cli.browser_preview_signins"] = preview


def build_parser():
    parser = argparse.ArgumentParser(prog="hermes")
    subs = parser.add_subparsers(dest="cmd")
    live.build_browser_parser(subs)
    return parser


def option_strings(parser):
    return [s for a in parser._actions for s in a.option_strings if s not in ("-h", "--help")]


def parser_shape(parser):
    browser_parser = parser._subparsers._group_actions[0].choices["browser"]
    group = browser_parser._subparsers._group_actions[0]
    hidden = {a.dest: a.help for a in group._choices_actions}
    return {
        "subcommands": list(group.choices),
        "close_options": option_strings(group.choices["close-profile"]),
        "export_options": option_strings(group.choices["export-signins"]),
        "export_hidden": hidden.get("export-signins") is argparse.SUPPRESS,
        "has_dispatch": browser_parser.get_default("func") is not None,
    }


def dispatch(parser, r):
    STATE.clear()
    STATE.update({"detected": r.get("detected"), "profile": r.get("profile") or {},
                  "closed": r.get("closed"), "msg": r.get("msg"),
                  "export_rc": r.get("export_rc", 0)})
    argv = ["browser"]
    if r.get("action"):
        argv.append(r["action"])
    if r.get("browser"):
        argv += ["--browser", r["browser"]]
    ns = parser.parse_args(argv)
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        rc = ns.func(ns)
    stdout = out.getvalue()
    if stdout.startswith("usage:"):
        stdout = "<HELP>"
    return {"rc": rc, "stdout": stdout, "stderr": err.getvalue()}


def expected(r, parser):
    f = r.get("func")
    if f == "parser_shape":
        return parser_shape(parser)
    if f == "dispatch":
        return dispatch(parser, r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    install_stubs()
    parser = build_parser()
    cases = mism = 0
    wanted = {"parser_shape", "dispatch"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r, parser)
        except SystemExit as exc:
            mism += 1
            print("MISMATCH %s case=%r argparse-exit=%r" % (r.get("func"), r.get("case"), exc.code),
                  file=sys.stderr)
            continue
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
