#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/sms/gateway_env.py
(gate w22_gatewayenv_8633ba).

The module is live data plus two behaviours, all executed live here:

  enable_credentials -- tuple(live.ENV_ENABLE_CREDENTIALS)
  overrides          -- each live step rendered: an EnvCred by its dataclass
                        fields (naming its `then` callable), a home step by the
                        functools.partial keywords `home_step` built.
  authz / authz_get  -- live AUTHZ, and AUTHZ.get(key) for the fixture key.
  cred_active        -- `all(env_first(group) for group in creds)` over the live
                        EnvCred's own creds, with the fixture SID in os.environ.
  api_key            -- live `_api_key(config, sms_config)` with the fixture
                        token in os.environ; the assigned value is read back.

os.environ is the environment seam; it is set and restored per case.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "sms" / "gateway_env.py"
GATE = "w22_gatewayenv_8633ba"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("gateway_env_live", SRC)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

from gateway.platform_env import env_first  # noqa: E402

WANTED = {"enable_credentials", "overrides", "authz", "authz_get",
          "cred_active", "api_key"}


def step_view(step):
    if isinstance(step, live.EnvCred):
        return {
            "kind": "cred",
            "platform": step.platform.value,
            "creds": list(step.creds),
            "token": step.token,
            "fixed": list(step.fixed),
            "optional": list(step.optional),
            "optional_stripped": list(step.optional_stripped),
            "warn_missing": step.warn_missing,
            "then": getattr(step.then, "__name__", None),
            "home": step.home,
            "home_strip": step.home_strip,
        }
    keywords = dict(step.keywords)
    return {
        "kind": "home",
        "platform": keywords["platform"].value,
        "env_base": keywords["env_base"],
        "strip": keywords["strip"],
    }


def with_env(name, value):
    saved = os.environ.get(name)
    if value is None:
        os.environ.pop(name, None)
    else:
        os.environ[name] = value
    return saved


def restore_env(name, saved):
    if saved is None:
        os.environ.pop(name, None)
    else:
        os.environ[name] = saved


def expected(r):
    func = r.get("func")
    if func == "enable_credentials":
        return list(live.ENV_ENABLE_CREDENTIALS)
    if func == "overrides":
        return [step_view(step) for step in live.ENV_OVERRIDES]
    if func == "authz":
        return dict(live.AUTHZ)
    if func == "authz_get":
        return live.AUTHZ.get(r.get("key"))
    if func == "cred_active":
        saved = with_env("TWILIO_ACCOUNT_SID", r.get("sid"))
        try:
            return bool(all(env_first(group) for group in live.ENV_OVERRIDES[0].creds))
        finally:
            restore_env("TWILIO_ACCOUNT_SID", saved)
    if func == "api_key":
        saved = with_env("TWILIO_AUTH_TOKEN", r.get("token"))
        try:
            sms_config = SimpleNamespace()
            live._api_key(None, sms_config)
            return sms_config.api_key
        finally:
            restore_env("TWILIO_AUTH_TOKEN", saved)
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
