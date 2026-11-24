#!/usr/bin/env python3
"""Differential oracle for plugins/config_backends/remote/credentials.py (gate w26_credential_0a5825).

The plane credential is resolved from the environment (on-prem IdP) and from
auth.json (Cloud); this compares the environment half, with ``os.environ``
patched to exactly the request's ``env`` object so the live ``_idp_env`` /
``credential_kind`` run unchanged:

  * ``_idp_env``         -- the four GATEWAY_RELAY_IDP_* values, stripped;
  * ``credential_kind``  -- idp when a token_url is configured, else nous;
  * ``_idp_token``'s form body (grant_type / client / optional scope);
  * ``_idp_token``'s cache TTL -- ``max(ttl - 30, 0)`` from ``expires_in``;
  * the env-name blocklist and the module constants.

The OAuth2 request, the token cache and the clock stay seams; only the values
their answers decide are compared.
"""
import json
import os
import sys
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_credential_0a5825"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.config_backends.remote import credentials as live  # noqa: E402

WANTED = {"idp_env", "credential_kind", "idp_form", "cache_ttl_offset", "constants"}

ENV_SUFFIXES = tuple(k.upper() for k in live._IDP_KEYS)


@contextmanager
def patched_env(env):
    """os.environ carrying exactly the request's GATEWAY_RELAY_IDP_* values."""
    env = env if isinstance(env, dict) else {}
    names = [live.IDP_ENV_PREFIX + suffix for suffix in ENV_SUFFIXES]
    saved = {name: os.environ.get(name) for name in names}
    try:
        for name in names:
            if name in env:
                os.environ[name] = str(env[name])
            else:
                os.environ.pop(name, None)
        yield
    finally:
        for name, value in saved.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value


def idp_env(r):
    with patched_env(r.get("env")):
        return live._idp_env()


def credential_kind(r):
    with patched_env(r.get("env")):
        return live.credential_kind()


def idp_form(r):
    with patched_env(r.get("env")):
        env = live._idp_env()
        form = {"grant_type": "client_credentials", "client_id": env["client_id"],
                "client_secret": env["client_secret"]}
        if env["scope"]:
            form["scope"] = env["scope"]
        return form


def cache_ttl_offset(r):
    expires_in = r.get("expires_in")
    ttl = float(expires_in) if isinstance(expires_in, int | float) and expires_in > 0 else 60.0
    return max(ttl - 30.0, 0.0)


def constants():
    return {
        "idp_env_prefix": live.IDP_ENV_PREFIX,
        "idp_keys": list(live._IDP_KEYS),
        "plane_credential_env_names": sorted(live.PLANE_CREDENTIAL_ENV_NAMES),
        "http_timeout_s": live._HTTP_TIMEOUT_S,
    }


def expected(r):
    f = r.get("func")
    if f == "idp_env":
        return idp_env(r)
    if f == "credential_kind":
        return credential_kind(r)
    if f == "idp_form":
        return idp_form(r)
    if f == "cache_ttl_offset":
        return cache_ttl_offset(r)
    if f == "constants":
        return constants()
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
