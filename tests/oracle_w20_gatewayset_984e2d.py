#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_setup_service.py (w20_gatewayset_984e2d).

The live module is imported from the quarry behind stubbed `hermes_cli.config`,
`hermes_cli.gateway`, `hermes_cli.gateway_runtime` and `hermes_constants`
packages: every service-manager call, printed line and saved config is recorded
in the same vocabulary the C port emits. Each fixture line scripts that state
and the two prompt answers.
"""
import copy
import importlib.util
import json
import os
import subprocess
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

LINES = sys.stdin.read().splitlines()


class UserSystemdUnavailableError(Exception):
    pass


class SystemScopeRequiresRootError(Exception):
    pass


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


for pkg in ("hermes_cli",):
    _mk(pkg)

NAME = "w20_gatewayset_984e2d_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "hermes_cli", "gateway_setup_service.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

ERRORS = {"UserSystemdUnavailableError": UserSystemdUnavailableError,
          "SystemScopeRequiresRootError": SystemScopeRequiresRootError,
          "SystemExit": SystemExit,
          "CalledProcessError": subprocess.CalledProcessError,
          "RuntimeError": RuntimeError}


class Rec:
    def __init__(self):
        self.messages = []
        self.calls = []
        self.saved = None
        self.recorded = None


class WindowsCli:
    def __init__(self, gw):
        self.gw = gw

    def install(self, force=False, start_now=False, start_on_login=False):
        self.gw._raise("install")
        self.gw.rec.calls.append({"call": "_gw_windows.install", "force": bool(force),
                                  "start_now": bool(start_now),
                                  "start_on_login": bool(start_on_login)})
        self.gw.installed = self.gw.state.get("installed_after_install", True)

    def start(self):
        self.gw._raise("start")
        self.gw.rec.calls.append({"call": "_gw_windows.start"})


class Gw:
    """hermes_cli.gateway, scripted by the fixture's `gw` object."""

    def __init__(self, state, rec, answers, store):
        self.state = state if isinstance(state, dict) else {}
        self.rec = rec
        self.answers = answers
        self.store = store
        self.installed = bool(self.state.get("installed", False))
        self.UserSystemdUnavailableError = UserSystemdUnavailableError
        self.SystemScopeRequiresRootError = SystemScopeRequiresRootError

    def _raise(self, op):
        if self.state.get("raise_at") == op:
            cls = ERRORS.get(self.state.get("error_type", "RuntimeError"), RuntimeError)
            msg = self.state.get("error_message", "boom")
            raise cls(msg) if cls is not SystemExit else SystemExit(msg)

    # printing
    def print_info(self, text):
        self.rec.messages.append({"kind": "info", "text": text})

    def print_warning(self, text):
        self.rec.messages.append({"kind": "warning", "text": text})

    def print_success(self, text):
        self.rec.messages.append({"kind": "success", "text": text})

    def print_error(self, text):
        self.rec.messages.append({"kind": "error", "text": text})

    def _print_indented(self, text, printer):
        self.rec.calls.append({"call": "_print_indented", "text": text})

    def _print_system_scope_remediation(self, mode):
        self.rec.calls.append({"call": "_print_system_scope_remediation", "mode": mode})

    def print_systemd_scope_conflict_warning(self):
        self.rec.calls.append({"call": "print_systemd_scope_conflict_warning"})

    # prompts
    def prompt_yes_no(self, question, default=False):
        answer = (self.answers.get("start_now", False)
                  if question == "  Start the gateway now?" else self.answers.get("prompt", False))
        self.rec.calls.append({"call": "prompt_yes_no", "question": question,
                               "default": bool(default), "answer": bool(answer)})
        return bool(answer)

    # environment
    def supports_systemd_services(self):
        return bool(self.state.get("supports_systemd", False))

    def is_macos(self):
        return bool(self.state.get("is_macos", False))

    def is_windows(self):
        return bool(self.state.get("is_windows", False))

    def _is_service_running(self):
        self._raise("running")
        return bool(self.state.get("running", False))

    def _served_profile_needs_no_service(self):
        self._raise("needs_no_service")
        return bool(self.state.get("needs_no_service", False))

    def _is_service_installed(self):
        self._raise("installed")
        return bool(self.installed)

    def has_conflicting_systemd_units(self):
        self._raise("conflict")
        return bool(self.state.get("conflicting_units", False))

    # installs / starts
    def systemd_install(self, force=False, non_interactive=False):
        self._raise("install")
        self.rec.calls.append({"call": "systemd_install", "force": bool(force),
                               "non_interactive": bool(non_interactive)})
        self.installed = self.state.get("installed_after_install", True)

    def launchd_install(self, force=False):
        self._raise("install")
        self.rec.calls.append({"call": "launchd_install", "force": bool(force)})
        self.installed = self.state.get("installed_after_install", True)

    def install_linux_gateway_from_setup(self, force=False, enable_on_startup=True):
        self._raise("install")
        scope = self.state.get("setup_scope", "user")
        did = bool(self.state.get("setup_did_install", True))
        self.rec.calls.append({"call": "install_linux_gateway_from_setup", "force": bool(force),
                               "enable_on_startup": bool(enable_on_startup),
                               "scope": scope, "did_install": did})
        if did:
            self.installed = self.state.get("installed_after_install", True)
        return scope, did

    def _setup_service_action(self, action, failed_label=None, system=False):
        self.rec.calls.append({"call": "_setup_service_action", "action": action,
                               "failed_label": failed_label, "system": bool(system)})

    def systemd_start(self):
        self._raise("start")
        self.rec.calls.append({"call": "systemd_start"})

    def launchd_start(self):
        self._raise("start")
        self.rec.calls.append({"call": "launchd_start"})

    def get_hermes_home(self):
        return "/tmp/hermes-home"

    def _gw_windows(self):
        return WindowsCli(self)


def build_env(state, rec, answers, store):
    """Install the stub packages and return the gateway module object."""
    gw = Gw(state, rec, answers, store)

    def load_config():
        return copy.deepcopy(store["cfg"])

    def save_config(cfg):
        store["cfg"] = copy.deepcopy(cfg)
        rec.saved = cfg

    cfg_mod = _mk("hermes_cli.config", load_config=load_config, save_config=save_config)
    gw_mod = _mk("hermes_cli.gateway", **{k: getattr(gw, k) for k in dir(gw)
                                          if not k.startswith("__")})
    runtime_mod = _mk("hermes_cli.gateway_runtime", ensure_gateway_runtime=lambda home, timeout=5.0: _runtime(state, rec))
    const_mod = _mk("hermes_constants", is_container=lambda: bool(state.get("in_container", False)))
    pkg = sys.modules["hermes_cli"]
    pkg.config = cfg_mod
    pkg.gateway = gw_mod
    pkg.gateway_runtime = runtime_mod
    sys.modules["hermes_constants"] = const_mod
    return gw


def _runtime(state, rec):
    rt = state.get("runtime") if isinstance(state.get("runtime"), dict) else {}
    rec.calls.append({"call": "ensure_gateway_runtime", "state": rt.get("state", "pending"),
                      "reason_code": rt.get("reason_code") if rt.get("reason_code") else None})
    return types.SimpleNamespace(state=rt.get("state", "pending"),
                                 reason_code=rt.get("reason_code"))


class Stdin:
    def __init__(self, tty):
        self.tty = tty

    def isatty(self):
        return self.tty


def run_case(r):
    state = r.get("gw") if isinstance(r.get("gw"), dict) else {}
    state = dict(state)
    runtime = r.get("runtime")
    if isinstance(runtime, dict):
        state["runtime"] = runtime
    rec = Rec()
    store = {"cfg": copy.deepcopy(r.get("stored")) if isinstance(r.get("stored"), dict) else {}}
    answers = {"prompt": bool(r.get("prompt_answer", False)),
               "start_now": bool(r.get("start_now_answer", False))}
    config = copy.deepcopy(r.get("config")) if r.get("config") is not None else None
    real_stdin = sys.stdin
    sys.stdin = Stdin(bool(r.get("isatty", False)))
    build_env(state, rec, answers, store)
    orig_record = live.record_service_choice

    def spy(choice, config=None):
        rec.recorded = choice
        return orig_record(choice, config)

    live.record_service_choice = spy
    out = {"ok": True, "error": None, "messages": rec.messages, "calls": rec.calls,
           "recorded": rec.recorded, "saved": rec.saved, "config": config}
    f = r.get("func")
    if f == "wants_service_install":
        out["decision"] = False          # the C record carries the field on every path
    if f == "ensure_gateway_service":
        out["result"] = False
    try:
        if f == "record_service_choice":
            # The direct call is the function under test, not a nested one.
            orig_record(r.get("choice"), config)
        elif f == "wants_service_install":
            out["decision"] = bool(live.wants_service_install(
                interactive=bool(r.get("interactive", False)),
                install=bool(r.get("install", False)), config=config))
        elif f == "ensure_gateway_service":
            out["result"] = bool(live.ensure_gateway_service(
                r.get("context", "setup"),
                interactive=bool(r.get("interactive", False)),
                install=bool(r.get("install", False)), config=config))
        elif f == "wizard_install_service":
            live._wizard_install_service(r.get("backend", "windows"))
        else:
            raise AssertionError("unknown func %r" % (f,))
    except Exception as exc:  # noqa: BLE001 - a live raise is part of the record
        out["ok"] = False
        out["error"] = {"type": type(exc).__name__, "message": str(exc)}
    finally:
        live.record_service_choice = orig_record
        sys.stdin = real_stdin
    out["recorded"] = rec.recorded
    out["saved"] = rec.saved
    out["config"] = config
    return out


cases = mism = 0
for line in LINES:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = run_case(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_gatewayset_984e2d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
