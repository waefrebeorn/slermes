#!/usr/bin/env python3
"""Differential oracle for plugins/cron_providers/chronos/verify.py
(gate w25_verify_1e9aa1).

The live verifier is called for real.  ``jwt`` is a stand-in module whose
``decode`` records the exact kwargs the live code chose and then answers with
the fixture's claims (or raises), and ``_get_jwk_client`` records that the JWKS
path was taken.  So the guards, the asymmetric-only decode plan, the
``exp``/``aud`` requirement, the issuer rule and the ``purpose`` claim test all
run live; only the crypto and the JWKS fetch are fixed inputs.

Reported shape:
    verify -> {"claims": <claims|null>, "key_kind": "jwks"|"inline"|null}
    plan   -> {"algorithms", "audience", "leeway", "options_require", "issuer"}
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_verify_1e9aa1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.cron_providers.chronos import verify as live  # noqa: E402

WANTED = {"verify", "plan", "constants"}
JWKS_KEY = "ORACLE-JWKS-SIGNING-KEY"

CASE = {}


class _JWKClient:
    def get_signing_key_from_jwt(self, token):
        return types.SimpleNamespace(key=JWKS_KEY)


class _JWT:
    """Stands in for PyJWT: records the plan, then obeys the fixture."""

    def __init__(self):
        self.decode_calls = []

    def decode(self, token, key, **kwargs):
        self.decode_calls.append({"token": token, "key": key, "kwargs": kwargs})
        spec = CASE.get("decode") or {}
        if not spec.get("ok"):
            raise ValueError("oracle: signature verification failed")
        return spec.get("claims")


def _install(req):
    CASE.clear()
    CASE.update(req)
    fake = _JWT()
    module = types.ModuleType("jwt")
    module.decode = fake.decode
    module.PyJWKClient = _JWKClient
    sys.modules["jwt"] = module
    seen = {"jwks": False}

    def get_jwk_client(url):
        seen["jwks"] = True
        return _JWKClient()

    live._get_jwk_client = get_jwk_client
    return fake, seen


def _kwargs_for(req):
    kwargs = {
        "token": req.get("token"),
        "expected_audience": req.get("audience"),
    }
    if "jwks_or_key" in req:
        kwargs["jwks_or_key"] = req["jwks_or_key"]
    if "issuer" in req:
        kwargs["issuer"] = req["issuer"]
    if "leeway" in req:
        kwargs["leeway_seconds"] = req["leeway"]
    return kwargs


def verify(req):
    fake, seen = _install(req)
    claims = live.verify_nas_fire_token(**_kwargs_for(req))
    decoded = bool(fake.decode_calls)
    kind = "jwks" if seen["jwks"] else ("inline" if decoded else None)
    return {"claims": claims, "key_kind": kind}


def plan(req):
    fake, seen = _install(req)
    live.verify_nas_fire_token(**_kwargs_for(req))
    if not fake.decode_calls:
        raise AssertionError("decode was never called for this plan case")
    kwargs = fake.decode_calls[-1]["kwargs"]
    return {
        "algorithms": kwargs["algorithms"],
        "audience": kwargs["audience"],
        "leeway": kwargs["leeway"],
        "options_require": kwargs["options"]["require"],
        "issuer": kwargs.get("issuer"),
    }


def constants():
    return {
        "fire_purpose": live._FIRE_PURPOSE,
        "algorithms": ["RS256", "RS384", "RS512", "ES256", "ES384"],
        "required_claims": 2,
        "default_leeway_seconds": 30,
        "verifier": live.get_fire_verifier().__name__,
    }


def expected(req):
    func = req.get("func")
    if func == "verify":
        return verify(req)
    if func == "plan":
        return plan(req)
    if func == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (func,))


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
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
