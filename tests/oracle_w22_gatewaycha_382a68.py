#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_chat_startup.py
(gate w22_gatewaycha_382a68).

The two entry points are live Python; only their seams are stubbed:

  hermes_cli.gateway_chat          bypass_launch, launch_from_args
  hermes_cli.main                  _has_any_provider_configured,
                                   _first_run_setup_guard,
                                   _confirm_startup_expensive_model_override
  hermes_cli.free_tier_bootstrap   run_bootstrap
  hermes_constants                 emit_partial_update_hint

Those modules are injected into sys.modules per case, so no profile, no
config.yaml and no gateway connection is touched. HERMES_TUI_GATEWAY_URL is the
environment seam and is set/restored per case. An import-failure case puts None
in sys.modules["hermes_cli.main"], which makes the real import block raise
ImportError.
"""
import importlib.util
import json
import os
import sys
import types
from argparse import Namespace
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "gateway_chat_startup.py"
GATE = "w22_gatewaycha_382a68"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("gateway_chat_startup_live", SRC)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"ensure", "launch"}
STUB_NAMES = ("hermes_cli", "hermes_cli.gateway_chat", "hermes_cli.main",
              "hermes_cli.free_tier_bootstrap", "hermes_constants")


def make_stubs(case, log):
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = []
    gw = types.ModuleType("hermes_cli.gateway_chat")
    main = types.ModuleType("hermes_cli.main")
    boot = types.ModuleType("hermes_cli.free_tier_bootstrap")
    consts = types.ModuleType("hermes_constants")

    def bypass_launch(_args):
        log["bypass"] = 1
        return bool(case.get("bypass"))

    def launch_from_args(_args):
        log["launch"] = 1
        return case.get("code", 0)

    def provider_configured():
        log["provider"] = 1
        return bool(case.get("provider"))

    def setup_guard(_args):
        log["guard"] = 1

    def confirm(_args):
        log["confirm"] = 1

    def bootstrap(announce=True):
        log["bootstrap"] = 1

    def hint(_exc):
        log["hint"] = 1
        return bool(case.get("hint_handled"))

    gw.bypass_launch = bypass_launch
    gw.launch_from_args = launch_from_args
    main._has_any_provider_configured = provider_configured
    main._first_run_setup_guard = setup_guard
    main._confirm_startup_expensive_model_override = confirm
    boot.run_bootstrap = bootstrap
    consts.emit_partial_update_hint = hint
    modules = {"hermes_cli": pkg, "hermes_cli.gateway_chat": gw,
               "hermes_cli.main": main, "hermes_cli.free_tier_bootstrap": boot,
               "hermes_constants": consts}
    if case.get("import_ok") is False:
        modules["hermes_cli.main"] = None
    return modules


@contextmanager
def injected(modules):
    saved = {name: sys.modules.get(name, KeyError) for name in STUB_NAMES}
    sys.modules.update(modules)
    try:
        yield
    finally:
        for name, value in saved.items():
            if value is KeyError:
                sys.modules.pop(name, None)
            elif value is None:
                sys.modules[name] = None
            else:
                sys.modules[name] = value


@contextmanager
def env(name, value):
    saved = os.environ.get(name)
    if value is None:
        os.environ.pop(name, None)
    else:
        os.environ[name] = value
    try:
        yield
    finally:
        if saved is None:
            os.environ.pop(name, None)
        else:
            os.environ[name] = saved


def ensure(case):
    log = {}
    args = Namespace(**(case.get("args") or {}))
    with injected(make_stubs(case, log)), env("HERMES_TUI_GATEWAY_URL", case.get("gateway_url")):
        ready = live.ensure_launch_provider(args)
    return {
        "ready": bool(ready),
        "bootstrap_called": bool(log.get("bootstrap")),
        "provider_checked": bool(log.get("provider")),
        "guard_called": bool(log.get("guard")),
    }


def launch(case):
    log = {}
    args = Namespace(**(case.get("args") or {}))
    with injected(make_stubs(case, log)):
        try:
            code = live.launch_gateway_chat(args)
        except ImportError:
            return {
                "outcome": "raised",
                "confirmed": bool(log.get("confirm")),
                "hint_called": bool(log.get("hint")),
            }
    return {
        "outcome": "returned",
        "code": code,
        "confirmed": bool(log.get("confirm")),
        "hint_called": bool(log.get("hint")),
    }


def expected(r):
    func = r.get("func")
    if func == "ensure":
        return ensure(r)
    if func == "launch":
        return launch(r)
    raise AssertionError("unknown func %r" % (func,))


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
