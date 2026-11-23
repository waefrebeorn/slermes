#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_xai.py (gate w7_authxai_4eac9a)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# Parent-package bootstrap and scripted collaborators for the module-scope
# imports. The helpers under test never touch network or the auth store.
_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = [QUARRY + "/hermes_cli"]
sys.modules.setdefault("hermes_cli", _pkg)

_codex = types.ModuleType("hermes_cli.auth_codex")
_codex._load_auth_store_maybe_locked = lambda lock=True: {}
_codex._refresh_payload_access_token = lambda *a, **k: ({}, "")
sys.modules.setdefault("hermes_cli.auth_codex", _codex)

_const = types.ModuleType("hermes_cli.auth_constants")
class AuthError(Exception):
    def __init__(self, message, code, relogin=False):
        super().__init__(message)
        self.message, self.code, self.relogin = message, code, relogin
_const.AuthError = AuthError
_const.AUTH_LOCK_TIMEOUT_SECONDS = 5
_const.DEFAULT_XAI_OAUTH_BASE_URL = "https://api.x.ai/v1"
_const.DEVICE_CODE_GRANT_TYPE = "urn:ietf:params:oauth:grant-type:device_code"
_const.XAI_ACCESS_TOKEN_REFRESH_SKEW_SECONDS = 3600
_const.XAI_OAUTH_CLIENT_ID = "xai-client"
_const.XAI_OAUTH_DEVICE_CODE_URL = "https://x.ai/device"
_const.XAI_OAUTH_DISCOVERY_URL = "https://x.ai/.well-known/openid-configuration"
_const.XAI_OAUTH_SCOPE = "openid"
_const._FORM_JSON_HEADERS = {}
_const._xai_err = lambda msg, code, relogin=False: AuthError(msg, code, relogin)
_const.httpx = types.ModuleType("httpx")
sys.modules.setdefault("hermes_cli.auth_constants", _const)

_utils = types.ModuleType("utils")
_utils.env_float = lambda key, default: float(default)
sys.modules.setdefault("utils", _utils)

spec = importlib.util.spec_from_file_location(
    "w7_authxai_live", QUARRY + "/hermes_cli/auth_xai.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w7_authxai_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "clean":
        return live._clean(r.get("value"))
    if f == "token_pair":
        return list(live._token_pair(r.get("tokens")))
    if f == "state_ok":
        return live._xai_oauth_state_has_usable_tokens(r.get("state"))
    if f == "origin":
        return live._is_xai_origin_host(r.get("host") or "")
    if f == "url_problem":
        return list(live._xai_url_problem(r.get("url") or ""))
    if f == "jwt_exp":
        return live._xai_jwt_exp(r.get("token"))
    if f == "tokens_from":
        return live._xai_tokens_from_payload(r.get("payload") or {},
                    r.get("access_token"), r.get("fallback_refresh") or "")
    if f == "validate_base":
        return live._xai_validate_inference_base_url(r.get("value") or "",
                    fallback=r.get("fallback") or "")
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w7_authxai_4eac9a: live raised %r case %r" % (exc, r.get("case")),
              file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH w7_authxai_4eac9a case=%s func=%s py=%r c=%r" %
              (r.get("case"), r.get("func"), py, got), file=sys.stderr)

print("ORACLE w7_authxai_4eac9a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
