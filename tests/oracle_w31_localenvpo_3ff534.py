#!/usr/bin/env python3
"""Differential oracle for tools/environments/local_env_policy.py (gate w31_localenvpo_3ff534).

The live module is imported as a package module and only its *pure* functions
are called.  The ones whose arguments are sets the module derives at import
time (the provider/adapter blocklist, the platform gate owners) are called with
the fixture's sets passed explicitly through the same parameters upstream
already exposes (`_registered`, `_prefixes`), so exactly the value->value
decision is compared:

  * snake_field / _snake_field            camelCase -> snake_case folding
  * matches_terminal_first_party_prefix    BUZZ_* name check
  * is_hermes_internal_secret              dynamic Hermes credential names
  * is_provider_env_blocklisted            provider/tool credential names
  * is_profile_gate_env                    platform authorization gates
  * carry_gateway_profile_pin              supervisor marker -> profile pin
  * strip_supervised_process_identity      pin survives, identity does not
  * strip_profile_gate_env                 gates dropped
  * config_declared_key_envs               key_env-style pointers in raw config
  * config_credential_env_refs             ${VAR} refs in credential fields
  * constants                              the module's own literal tables
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_localenvpo_3ff534"

# The gate runs under a login shell that carries no HOME, and the live module's
# import chain (hermes_constants.get_hermes_home -> Path.home) needs one.  Any
# already-set HOME wins, so the temp-home proof step is untouched.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)

# This repo's tests/ carries its own `tools`, `gateway` and `hermes_cli`
# packages and the script's directory is sys.path[0].  The quarry must come
# first or the shadow package wins and a different module is compared.
sys.path.insert(0, str(QUARRY))
for _stale in ("tools", "tools.environments", "gateway", "hermes_cli", "agent",
               "persistence", "hermes_constants"):
    _mod = sys.modules.get(_stale)
    if _mod is not None and not str(getattr(_mod, "__file__", "")).startswith(str(QUARRY)):
        del sys.modules[_stale]

try:
    from tools.environments import local_env_policy as live  # noqa: E402
except ImportError:  # blocked native extension: install the seam stubs, retry
    sys.path.append(str(Path(__file__).resolve().parent))
    from hermes_seam_stubs import install as _install_seams  # noqa: E402

    _install_seams()
    from tools.environments import local_env_policy as live  # noqa: E402

WANTED = {
    "snake_field", "matches_terminal_first_party_prefix", "is_hermes_internal_secret",
    "is_provider_env_blocklisted", "is_profile_gate_env", "carry_gateway_profile_pin",
    "strip_supervised_process_identity", "strip_profile_gate_env",
    "config_declared_key_envs", "config_credential_env_refs", "constants",
}


def _env(req):
    env = req.get("env")
    return dict(env) if isinstance(env, dict) else {}


def _prefixes(req):
    p = req.get("prefixes")
    return frozenset(p) if isinstance(p, list) else None


def snake_field(req):
    return live._snake_field(req.get("key"))


def matches_prefix(req):
    return live._matches_terminal_first_party_prefix(req.get("name"))


def internal_secret(req):
    return live._is_hermes_internal_secret(req.get("key"))


def blocklisted(req):
    # `_registered` is a real upstream parameter: pass the fixture's set so the
    # per-profile sets are inputs, not whatever this process happens to have.
    reg = req.get("registered")
    if not isinstance(reg, list):
        raise AssertionError("is_provider_env_blocklisted needs an explicit 'registered' list")
    return live._is_provider_env_blocklisted(req.get("name"), frozenset(reg))


def profile_gate(req):
    return live.is_profile_gate_env(req.get("name"), _prefixes(req))


def carry_pin(req):
    return live.carry_gateway_profile_pin(_env(req))


def strip_identity(req):
    return live.strip_supervised_process_identity(_env(req))


def strip_gates(req):
    # strip_profile_gate_env takes no prefixes argument: it derives the owner
    # set itself.  The fixture records that set, so drift is caught here rather
    # than showing up as a mysterious output mismatch.
    prefixes = req.get("prefixes")
    if not isinstance(prefixes, list):
        raise AssertionError("strip_profile_gate_env needs an explicit 'prefixes' list")
    if sorted(prefixes) != sorted(live._platform_gate_env_prefixes()):
        raise AssertionError("fixture prefixes drifted from _platform_gate_env_prefixes()")
    return live.strip_profile_gate_env(_env(req))


def declared_key_envs(req):
    return sorted(live._config_declared_key_envs(req.get("node")))


def credential_refs(req):
    return sorted(live._config_credential_env_refs(req.get("node")))


def constants(_req=None):
    return {
        "gateway_profile_pin_env": live.GATEWAY_PROFILE_PIN_ENV,
        "supervised_process_identity_vars": list(live._SUPERVISED_PROCESS_IDENTITY_VARS),
        "terminal_first_party_env_prefixes": list(live._TERMINAL_FIRST_PARTY_ENV_PREFIXES),
        "profile_gate_env_markers": list(live._PROFILE_GATE_ENV_MARKERS),
        "extra_gate_env_prefixes": sorted(live._EXTRA_GATE_ENV_PREFIXES),
        "bridged_env_marker_prefix": live.BRIDGED_ENV_MARKER_PREFIX,
        "static_provider_env_blocklist_size": len(live._STATIC_PROVIDER_ENV_BLOCKLIST),
    }


HANDLERS = {
    "snake_field": snake_field,
    "matches_terminal_first_party_prefix": matches_prefix,
    "is_hermes_internal_secret": internal_secret,
    "is_provider_env_blocklisted": blocklisted,
    "is_profile_gate_env": profile_gate,
    "carry_gateway_profile_pin": carry_pin,
    "strip_supervised_process_identity": strip_identity,
    "strip_profile_gate_env": strip_gates,
    "config_declared_key_envs": declared_key_envs,
    "config_credential_env_refs": credential_refs,
    "constants": constants,
}


def expected(req):
    func = req.get("func")
    handler = HANDLERS.get(func)
    if handler is None:
        raise AssertionError("unknown func %r" % (func,))
    return handler(req)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
