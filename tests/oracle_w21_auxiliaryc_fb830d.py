#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_client_registry.py
(gate w21_auxiliaryc_fb830d).

The live module is pure stdlib (logging + typing), so it imports by path and
the real `REGISTRY_AUTHTYPE_ARMS`, `_log_once_debug` and
`_resolve_minimax_oauth_arm` are driven directly. The arm's late imports are the
seams, stubbed in sys.modules:

  agent.auxiliary_client        AnthropicAuxiliaryClient, _AuxProbeClientStub,
                                _aux_probe_active, _get_aux_model_for_provider,
                                _normalize_resolved_model, _route_client
  agent.anthropic_credentials   anthropic_route_is_oauth
  agent.anthropic_adapter       build_anthropic_client  (ImportError = SDK absent)
  hermes_cli.auth               get_provider_auth_state,
                                resolve_minimax_oauth_runtime_credentials
  hermes_cli.auth_constants     AuthError

`_normalize_resolved_model` is the identity here (model normalisation is another
module's policy), so the ladder req.model -> aux model -> "MiniMax-M3" and the
base URL's strip().rstrip("/") stay the live code's own decisions.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "agent" / "auxiliary_client_registry.py"
GATE = "w21_auxiliaryc_fb830d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


class AuthError(Exception):
    pass


class RecLogger:
    def __init__(self):
        self.msgs = []

    def debug(self, msg, *args, **kwargs):
        self.msgs.append(("debug", (msg % args) if args else msg))

    def warning(self, msg, *args, **kwargs):
        self.msgs.append(("warning", (msg % args) if args else msg))

    def info(self, *a, **k):
        pass


class StubProbeClient:
    kind = "probe"

    def __init__(self, api_key=None, base_url=None):
        self.api_key = api_key
        self.base_url = base_url


class StubAnthropicClient:
    kind = "anthropic"

    def __init__(self, real_client, model, token_provider, base_url, is_oauth=False):
        self.real_client = real_client
        self.model = model
        self.token_provider = token_provider
        self.base_url = base_url
        self.is_oauth = is_oauth


CASE = {"data": {}}
LOGGER = RecLogger()


def install_modules():
    """The late-import seams, reading the current case on every call."""

    def aux_probe_active():
        return bool(CASE["data"].get("probe"))

    def aux_model_for_provider(provider, **kwargs):
        return CASE["data"].get("aux_model") or ""

    def normalize_model(model_name, provider):
        return model_name

    def route_client(req, client, final_model):
        return (client, final_model)

    def get_provider_auth_state(name):
        CASE["trace"]["state"] += 1
        return CASE["data"].get("state")

    def resolve_creds(**kwargs):
        case = CASE["data"]
        err = case.get("creds_error")
        if err:
            text = (err.get("message") or "") if isinstance(err, dict) else str(err)
            kind = (err.get("type") if isinstance(err, dict) else "AuthError") or "AuthError"
            CASE["trace"]["creds_error"] = kind
            raise (AuthError if kind == "AuthError" else RuntimeError)(text)
        creds = case.get("creds")
        if not isinstance(creds, dict):
            return creds
        out = dict(creds)
        # `as_token_provider=True` hands back a callable bearer, never a string.
        if out.pop("api_key_callable", False):
            out["api_key"] = lambda: "bearer-token"
        return out

    def build_anthropic_client(token_provider, base_url):
        if CASE["data"].get("build_import_error"):
            CASE["trace"]["import"] += 1
            raise ImportError("anthropic SDK not installed")
        return SimpleNamespace(token_provider=token_provider, base_url=base_url)

    def route_is_oauth(base_url, token_provider):
        return False

    mods = {
        "agent": types.ModuleType("agent"),
        "agent.auxiliary_client": types.ModuleType("agent.auxiliary_client"),
        "agent.anthropic_credentials": types.ModuleType("agent.anthropic_credentials"),
        "agent.anthropic_adapter": types.ModuleType("agent.anthropic_adapter"),
        "hermes_cli": types.ModuleType("hermes_cli"),
        "hermes_cli.auth": types.ModuleType("hermes_cli.auth"),
        "hermes_cli.auth_constants": types.ModuleType("hermes_cli.auth_constants"),
    }
    mods["agent"].__path__ = []
    mods["hermes_cli"].__path__ = []
    ac = mods["agent.auxiliary_client"]
    ac.AnthropicAuxiliaryClient = StubAnthropicClient
    ac._AuxProbeClientStub = StubProbeClient
    ac._aux_probe_active = aux_probe_active
    ac._get_aux_model_for_provider = aux_model_for_provider
    ac._normalize_resolved_model = normalize_model
    ac._route_client = route_client
    mods["agent.anthropic_credentials"].anthropic_route_is_oauth = route_is_oauth
    mods["agent.anthropic_adapter"].build_anthropic_client = build_anthropic_client
    mods["hermes_cli.auth"].get_provider_auth_state = get_provider_auth_state
    mods["hermes_cli.auth"].resolve_minimax_oauth_runtime_credentials = resolve_creds
    mods["hermes_cli.auth_constants"].AuthError = AuthError
    sys.modules.update(mods)


install_modules()

_spec = importlib.util.spec_from_file_location("aux_client_registry_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["aux_client_registry_live"] = live
_spec.loader.exec_module(live)


def reason_of(msg):
    if not msg:
        return None
    if "not logged in" in msg:
        return "probe_absent"
    if "unavailable:" in msg:
        return "auth_error"
    if "unexpected" in msg:
        return "unexpected"
    if "credentials incomplete" in msg:
        return "incomplete"
    return "other"


def drive_minimax(case):
    CASE["data"] = case
    CASE["trace"] = {"state": 0, "creds_error": None, "import": 0}
    LOGGER.msgs = []
    live.logger = LOGGER
    live._LOGGED_MINIMAX_ABSENT_KEYS = set(case.get("seen") or [])
    live._LOGGED_MINIMAX_UNEXPECTED_KEYS = set(case.get("seen") or [])
    req = SimpleNamespace(model=case.get("req_model"), provider=case.get("provider") or "minimax-oauth")
    ret = live._resolve_minimax_oauth_arm(req)
    client, model = ret
    emitted = [m for _kind, m in LOGGER.msgs]
    if client is None:
        trace = CASE["trace"]
        if trace["state"]:
            reason = "probe_absent"
        elif trace["creds_error"] == "AuthError":
            reason = "auth_error"
        elif trace["creds_error"]:
            reason = "unexpected"
        elif trace["import"]:
            reason = "sdk_absent"
        else:
            reason = "incomplete"
        return {
            "status": "absent",
            "reason": reason,
            "base_url": None,
            "model": None,
            "kind": None,
            "debug": emitted[0] if emitted else None,
            "logged": bool(emitted),
        }
    return {
        "status": "ok",
        "reason": None,
        "base_url": getattr(client, "base_url", None),
        "model": model,
        "kind": getattr(client, "kind", None),
        "debug": None,
        "logged": False,
    }


def expected(r):
    f = r.get("func")
    if f == "arms":
        return {k: v.__name__ for k, v in live.REGISTRY_AUTHTYPE_ARMS.items()}
    if f == "arm_for":
        arm = live.REGISTRY_AUTHTYPE_ARMS.get(r.get("auth_type") or "")
        return arm.__name__ if arm is not None else None
    if f == "log_once":
        seen = set(r.get("seen") or [])
        flags = []
        for key in r.get("keys") or []:
            before = key in seen
            live._log_once_debug(seen, key, "probe %s", key)
            flags.append(not before)
        return {"logged": flags, "seen": sorted(seen)}
    if f == "minimax":
        return drive_minimax(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"arms", "arm_for", "log_once", "minimax"}
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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
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
