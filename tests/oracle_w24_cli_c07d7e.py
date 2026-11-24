#!/usr/bin/env python3
"""Differential oracle for plugins/google_meet/node/cli.py (gate w24_cli_c07d7e).

The live CLI module is imported from the Python quarry and its own handlers are
called with a stand-in for each seam -- the node registry, the node server and
the node client.  Nothing touches the registry file, binds a port or opens a
socket; what is compared is what the CLI prints and returns:

  * ``node_command`` for the dispatch table and the unknown-command refusal;
  * ``_cmd_list`` / ``_cmd_approve`` / ``_cmd_remove`` over a registry stub;
  * ``_cmd_run`` with ``NodeServer`` patched (banner only, then it returns);
  * ``_cmd_ping`` with a client that raises, and with an unknown node;
  * ``register_cli``'s argparse defaults and command table.
"""
import argparse
import contextlib
import io
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_cli_c07d7e"

# Import-time code in the live package resolves the user's home; a bare MSYS
# shell exports HOME but no USERPROFILE, which ``Path.home()`` refuses.
if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.google_meet.node import cli as live  # noqa: E402

WANTED = {"dispatch", "format_list", "approve_message", "remove_result",
          "run_banner", "ping_fail", "ping_missing", "constants"}


def dispatch(req):
    cmd = req.get("cmd")
    handler = live._COMMANDS.get(cmd or "")
    if handler is not None:
        return {"handler": handler.__name__, "rc": None, "error": None}
    err = io.StringIO()
    with contextlib.redirect_stderr(err):
        rc = live.node_command(SimpleNamespace(node_cmd=cmd))
    return {"handler": None, "rc": rc, "error": err.getvalue().rstrip("\n")}


class _ListRegistry:
    def __init__(self, nodes):
        self.nodes = nodes

    def list_all(self):
        return self.nodes


def format_list(req):
    nodes = req.get("nodes")
    nodes = nodes if isinstance(nodes, list) else []
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        live._cmd_list(SimpleNamespace(), _ListRegistry(nodes))
    return out.getvalue()


class _ApproveRegistry:
    def add(self, name, url, token):
        self.added = (name, url, token)


def approve_message(req):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        live._cmd_approve(SimpleNamespace(name=req.get("name"), url=req.get("url"),
                                          token=req.get("token") or ""),
                          _ApproveRegistry())
    return out.getvalue()


class _RemoveRegistry:
    def __init__(self, ok):
        self.ok = ok

    def remove(self, name):
        return self.ok


def remove_result(req):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        rc = live._cmd_remove(SimpleNamespace(name=req.get("name")),
                              _RemoveRegistry(bool(req.get("ok"))))
    return {"message": out.getvalue(), "rc": rc}


def run_banner(req):
    token = req.get("token")

    class FakeServer:
        def __init__(self, host, port, display_name):
            self.host, self.port, self.display_name = host, port, display_name

        def ensure_token(self):
            return token

        async def serve(self):
            return None

    args = SimpleNamespace(host=req.get("host"), port=req.get("port"),
                           display_name=req.get("display_name"))
    out = io.StringIO()
    with mock.patch.object(live, "NodeServer", FakeServer), contextlib.redirect_stdout(out):
        rc = live._cmd_run(args, None)
    assert rc == 0, "run returned %r" % (rc,)
    return out.getvalue()


class _PingRegistry:
    def __init__(self, entry):
        self.entry = entry

    def get(self, name):
        return self.entry


def ping_fail(req):
    message = req.get("error")

    class FakeClient:
        def __init__(self, url, token):
            pass

        def ping(self):
            raise RuntimeError(message)

    out = io.StringIO()
    with mock.patch.object(live, "NodeClient", FakeClient), contextlib.redirect_stdout(out):
        rc = live._cmd_ping(SimpleNamespace(name=req.get("name")),
                            _PingRegistry({"url": "ws://x", "token": "t"}))
    return {"stdout": out.getvalue(), "rc": rc}


def ping_missing(req):
    out = io.StringIO()
    with contextlib.redirect_stderr(out):
        rc = live._cmd_ping(SimpleNamespace(name=req.get("name")), _PingRegistry(None))
    return {"stderr": out.getvalue(), "rc": rc}


def constants(_req):
    parser = argparse.ArgumentParser(prog="node")
    live.register_cli(parser)
    run = parser.parse_args(["run"])
    action = parser._subparsers._group_actions[0]
    return {"default_host": run.host, "default_port": run.port,
            "default_display_name": run.display_name,
            "commands": list(action.choices)}


def expected(r):
    f = r.get("func")
    handlers = {"dispatch": dispatch, "format_list": format_list,
                "approve_message": approve_message, "remove_result": remove_result,
                "run_banner": run_banner, "ping_fail": ping_fail,
                "ping_missing": ping_missing, "constants": constants}
    if f not in handlers:
        raise AssertionError("unknown func %r" % (f,))
    return handlers[f](r)


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
