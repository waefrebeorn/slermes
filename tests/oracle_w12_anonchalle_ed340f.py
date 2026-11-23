#!/usr/bin/env python3
"""Differential oracle for hermes_cli/anon_challenge.py (w12_anonchalle_ed340f).

The live module is loaded with stubbed hermes_cli.anon_auth / auth_constants
(parent package bootstrapped). Only the pure parse/validation leaf functions
are called; presenting/polling/state stay out of scope.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

hermes_cli = types.ModuleType("hermes_cli")
hermes_cli.__path__ = []
sys.modules["hermes_cli"] = hermes_cli

anon_auth = types.ModuleType("hermes_cli.anon_auth")
anon_auth.ANON_CHALLENGE_REQUIRED = "challenge_required"
anon_auth.ANON_SIGNIN_REQUIRED = "signin_required"
anon_auth.ANON_FAILURE_COPY = {}
anon_auth._anon_headers = lambda: {}
anon_auth._mint_memo_key = lambda: "k"
sys.modules["hermes_cli.anon_auth"] = anon_auth

auth_constants = types.ModuleType("hermes_cli.auth_constants")


class AuthError(RuntimeError):
    def __init__(self, message, *, provider="", code=None, relogin_required=False,
                 retry_after=None, retryable=None):
        super().__init__(message)
        self.provider = provider
        self.code = code
        self.relogin_required = relogin_required
        self.retry_after = retry_after
        self.retryable = retryable


class _Httpx:
    Client = object
    HTTPError = Exception


auth_constants.AuthError = AuthError
auth_constants.httpx = _Httpx()
sys.modules["hermes_cli.auth_constants"] = auth_constants

SRC = Q + "/hermes_cli/anon_challenge.py"
spec = importlib.util.spec_from_file_location("anon_challenge_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["anon_challenge_live"] = live
spec.loader.exec_module(live)

CONST_KEYS = [
    "BROWSER_CAPABILITY", "CHALLENGE_EVENT", "CHALLENGE_COPY",
    "CHALLENGE_WAIT_SECONDS", "_POLL_MIN_SECONDS", "_POLL_MAX_SECONDS",
    "_EXPIRES_MIN_SECONDS", "_EXPIRES_MAX_SECONDS", "_EXPIRES_DEFAULT_SECONDS",
    "_MESSAGE_MAX_CHARS", "_GAVE_UP_COOLDOWN_SECONDS",
]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "server_message":
            py = live.server_message(r.get("value"))
        elif func == "same_origin":
            py = live._same_origin(r.get("url", ""), r.get("portal", ""))
        elif func == "number":
            py = live._number(r.get("value"), float(r.get("default", 0)))
        elif func == "parse":
            bc = live.parse_browser_challenge(r.get("payload"), r.get("portal", ""))
            py = bc.as_payload() if bc is not None else None
        elif func == "constants":
            py = {k: getattr(live, k) for k in CONST_KEYS}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w12_anonchalle_ed340f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
