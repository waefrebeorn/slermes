#!/usr/bin/env python3
"""Differential oracle for hermes_cli/runtime_provider_missing_key.py (gate w21_runtimepro_eb8ed1).

The live module is imported from the quarry and every function runs for real.
Three of them reach host state, and each is answered from the fixture in the
exact place upstream reads it:

  * the profile reads -- `hermes_constants.get_hermes_home`,
    `profile_name_for_home`, `display_hermes_home`, `profile_cli_selector` and
    `agent.secret_scope.serves_routed_profile` -- are module attributes patched
    for the duration of one case;
  * the process environment is real: `where_to_set_key` reads
    `os.environ.get(env_var)`, so a case that says the variable is exported here
    sets it and every other case removes it;
  * `agent.secret_scope.get_secret_str` is the secret store -- patched to the
    case's `secrets` map.

`_declared_custom_key` keeps the real `custom_entry_for_endpoint` /
`named_custom_entry`, so a fixture's `config` is normalized by upstream itself;
the fixture also carries the compatible-entry view the C port is handed, and
that view is checked against the live normalization so a drifting fixture fails
loudly.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "runtime_provider_missing_key.py"
GATE = "w21_runtimepro_eb8ed1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import agent.secret_scope as secret_scope  # noqa: E402
import hermes_constants  # noqa: E402
from hermes_cli.auth import AuthError  # noqa: E402
from hermes_cli.config_providers import get_compatible_custom_providers  # noqa: E402

_spec = importlib.util.spec_from_file_location("runtime_provider_missing_key_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["runtime_provider_missing_key_live"] = live
_spec.loader.exec_module(live)

_REAL = {
    "get_hermes_home": hermes_constants.get_hermes_home,
    "profile_name_for_home": hermes_constants.profile_name_for_home,
    "display_hermes_home": hermes_constants.display_hermes_home,
    "profile_cli_selector": hermes_constants.profile_cli_selector,
    "serves_routed_profile": secret_scope.serves_routed_profile,
    "get_secret_str": secret_scope.get_secret_str,
}

STATE = {"env": {}, "secrets": {}}


def _apply_env(spec):
    spec = spec if isinstance(spec, dict) else {}
    profile = spec.get("profile")
    routed = bool(spec.get("serves_routed"))
    hermes_constants.get_hermes_home = lambda: Path("C:/hermes-home")
    hermes_constants.profile_name_for_home = lambda home: profile
    hermes_constants.display_hermes_home = lambda: str(spec.get("display_home") or "")
    hermes_constants.profile_cli_selector = lambda: str(spec.get("cli_selector") or "")
    secret_scope.serves_routed_profile = lambda: routed
    live.get_secret_str = lambda name, default="": (STATE["secrets"] or {}).get(name, default)
    STATE["env"] = dict(spec.get("vars") or {})


def _reset():
    hermes_constants.get_hermes_home = _REAL["get_hermes_home"]
    hermes_constants.profile_name_for_home = _REAL["profile_name_for_home"]
    hermes_constants.display_hermes_home = _REAL["display_hermes_home"]
    hermes_constants.profile_cli_selector = _REAL["profile_cli_selector"]
    secret_scope.serves_routed_profile = _REAL["serves_routed_profile"]
    live.get_secret_str = _REAL["get_secret_str"]
    STATE["env"] = {}


def _with_env_var(name, present, fn):
    """`os.environ` is real: an exported variable is set, anything else removed."""
    if not name:
        return fn()
    had = name in os.environ
    saved = os.environ.get(name)
    if present:
        os.environ[name] = "exported-value"
    else:
        os.environ.pop(name, None)
    try:
        return fn()
    finally:
        if had:
            os.environ[name] = saved
        else:
            os.environ.pop(name, None)


def _entries_of(config):
    return get_compatible_custom_providers(config) or []


def _check_entries(r):
    """The fixture's entries must be the live normalization of its config."""
    if "config" not in r or "entries" not in r:
        return
    live_entries = _entries_of(r.get("config") or {})
    given = r.get("entries") or []
    if len(live_entries) != len(given):
        raise AssertionError("entries=%r but live normalization gives %r" % (given, live_entries))
    keys = ("name", "base_url", "key_env", "api_key_env", "provider_key")
    for mine, theirs in zip(given, live_entries):
        for key in keys:
            if (mine or {}).get(key) != (theirs or {}).get(key):
                raise AssertionError("entries %r disagree with live %r on %r" % (mine, theirs, key))


def expected(r):
    f = r.get("func")
    if f == "declared_key_env":
        return live._declared_key_env(r.get("entry"))
    if f == "declared_custom_key":
        _check_entries(r)
        label, key_env = live._declared_custom_key(str(r.get("requested_provider") or ""),
                                                   r.get("runtime") or {},
                                                   r.get("model_cfg") or {},
                                                   r.get("config") or {})
        return {"label": label, "key_env": key_env}
    if f == "where_to_set_key":
        env_var = str(r.get("env_var") or "")
        _apply_env({"profile": None, "serves_routed": r.get("serves_routed"),
                    "display_home": r.get("display_home"), "cli_selector": r.get("cli_selector"),
                    "vars": {env_var: 1} if r.get("env_has_var") else {}})
        # where_to_set_key derives the profile itself; pin it through hermes_constants.
        profile = r.get("profile")
        hermes_constants.profile_name_for_home = lambda home, _p=profile: _p
        try:
            return _with_env_var(env_var, bool(r.get("env_has_var")), lambda: live.where_to_set_key(env_var))
        finally:
            _reset()
    if f == "named_profile_key_hint":
        env_vars = r.get("env_vars") or []
        first = ""
        for value in env_vars:
            text = str(value or "")
            if text.strip():
                first = text
                break
        _apply_env({"profile": r.get("profile"), "serves_routed": r.get("serves_routed"),
                    "display_home": r.get("display_home"), "cli_selector": r.get("cli_selector"),
                    "vars": {first: 1} if r.get("env_has_var") else {}})
        try:
            return _with_env_var(first, bool(r.get("env_has_var")),
                                 lambda: live.named_profile_key_hint(env_vars))
        finally:
            _reset()
    if f == "raise_if_missing":
        _check_entries(r)
        STATE["secrets"] = dict(r.get("secrets") or {})
        _apply_env(r.get("env") or {})
        explicit = r.get("explicit_api_key")
        exported = [name for name, value in ((r.get("env") or {}).get("vars") or {}).items() if value]
        saved = {name: os.environ.get(name) for name in exported}
        for name in exported:
            os.environ[name] = "exported-value"
        try:
            live.raise_if_declared_key_missing(r.get("requested_provider"), r.get("runtime") or {},
                                               r.get("model_cfg") or {}, r.get("config") or {},
                                               explicit_api_key=explicit)
            return None
        except AuthError as exc:
            return {"status": "error", "type": "AuthError", "message": str(exc),
                    "provider": exc.provider, "code": exc.code}
        finally:
            for name, value in saved.items():
                if value is None:
                    os.environ.pop(name, None)
                else:
                    os.environ[name] = value
            _reset()
            STATE["secrets"] = {}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"declared_key_env", "declared_custom_key", "where_to_set_key",
              "named_profile_key_hint", "raise_if_missing"}
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
