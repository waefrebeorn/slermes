#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_declared_providers.py
(gate w21_authdeclar_1a88fd).

The live module runs unmodified; every IO edge is stubbed from the fixture:

* `hermes_cli.config.load_config` -> the fixture config, put through the LIVE
  `_expand_env_vars` with `_env_ref_lookup` bound to the fixture env map (the
  real loader expands `${VAR}` / `${env:VAR}` before returning);
* `get_env_value_prefer_dotenv`, `agent.secret_scope.get_secret_str` and
  `hermes_cli.runtime_provider_custom.get_secret_str` -> the env map;
* `hermes_constants.hermes_home_key`, `build_command_token_provider`,
  `materialize_probe_api_key` -> the key_cmd seam;
* `hermes_cli.runtime_provider.has_usable_secret` is the LIVE one from
  `hermes_cli.auth`; `_host_gated_env_key_candidates` and
  `_owning_entry_credential` are the fixture's declared-entry seam.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = Path("C:/Users/eman5/hermes-agent")
SRC = QUARRY / "hermes_cli" / "auth_declared_providers.py"
GATE = "w21_authdeclar_1a88fd"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import agent.command_token_source as cts
import agent.secret_scope as secret_scope
import hermes_cli.auth as auth
import hermes_cli.config as hcfg
import hermes_cli.runtime_provider as rp
import hermes_cli.runtime_provider_custom as rpc
import hermes_constants

_spec = importlib.util.spec_from_file_location("auth_declared_providers_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["auth_declared_providers_live"] = live
_spec.loader.exec_module(live)

ENV = {}
KEY_CMD = {}
KNOWN = False
HOST_GATED = []
DECLARED = False
ENTRY_KEY = ""


def _envget(name, default=""):
    value = ENV.get(name)
    return default if value is None else value


def install():
    hcfg._env_ref_lookup = lambda name: ENV.get(name)
    hcfg.load_config = lambda: hcfg._expand_env_vars(_config_copy)
    hcfg.get_env_value_prefer_dotenv = lambda name: ENV.get(name)
    secret_scope.get_secret_str = _envget
    rpc.get_secret_str = _envget
    auth.is_known_auth_provider = lambda provider_id: KNOWN
    rp.has_usable_secret = auth.has_usable_secret
    rp._host_gated_env_key_candidates = lambda base_url, ollama=True: list(HOST_GATED)
    rpc._owning_entry_credential = lambda rp_mod, base_url: (DECLARED, ENTRY_KEY)
    hermes_constants.hermes_home_key = lambda: "test-home"
    cts.build_command_token_provider = lambda cmd, name: {"cmd": cmd}
    cts.materialize_probe_api_key = lambda source: KEY_CMD.get(
        source.get("cmd") if isinstance(source, dict) else source, "")


_config_copy = {}

install()


def expected(r):
    global ENV, KEY_CMD, KNOWN, HOST_GATED, DECLARED, ENTRY_KEY, _config_copy
    func = r.get("func")
    if func == "default_url":
        return live.default_api_key_base_url(r.get("api_key"), r.get("default_url"),
                                             r.get("env_url"))
    if func == "origin":
        scheme, host, port = __import__("utils.urls", fromlist=["x"]).base_url_origin(
            r.get("base_url") or "")
        return {"scheme": scheme, "host": host, "port": port}
    if func == "relay":
        KNOWN = bool(r.get("known_provider"))
        return live.relay_probe_api_key(r.get("provider_id"), r.get("relay"),
                                        r.get("api_key"), r.get("cred_base_url"))
    if func == "resolve":
        ENV = dict(r.get("env") or {})
        KEY_CMD = dict(r.get("key_cmd_values") or {})
        install()
        _config_copy = r.get("config") or {}
        try:
            value = live.resolve_declared_api_key_provider(r.get("provider_id"))
        except auth.AuthError as exc:
            return {"status": "error", "type": type(exc).__name__, "message": str(exc),
                    "provider": exc.provider, "code": exc.code}
        return {"status": "ok", "value": value}
    if func == "custom":
        bundle = r.get("bundle") or {}
        ENV = dict(bundle.get("env") or {})
        owning = bundle.get("owning") or {}
        DECLARED = bool(owning.get("declared"))
        ENTRY_KEY = owning.get("key")
        HOST_GATED = list(bundle.get("host_gated") or [])
        KEY_CMD = dict(bundle.get("key_cmd_values") or {})
        install()
        return live.custom_catalog_probe_api_key(
            bundle.get("model_cfg") or {}, bundle.get("base_url"),
            minted=bool(bundle.get("minted", True)))
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    wanted = {"default_url", "origin", "relay", "resolve", "custom"}
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
        except BaseException as exc:  # noqa: BLE001 - reported, never fatal
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
