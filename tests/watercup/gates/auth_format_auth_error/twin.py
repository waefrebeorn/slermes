"""twin for auth_format_auth_error.

Imports the LIVE hermes_cli/auth.py::format_auth_error and traces the SAME
flat branch ladder as the C harness:
  B1 non-AuthError / B2 rate-limited / B3 relogin /
  B4 subscription_required (+B5 nous?) / B6 insufficient_credits (+B7 nous?) /
  B8 temporarily_unavailable / else.
Nous-provider entitlement inputs and the non-nous no-op
{subscription_expired,...} group are not traced (see gate.json notes).

Input encoding: "<code|provider|relogin|msg>" ('-' code = non-AuthError).
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

spec = ilu.spec_from_file_location(
    "wc_live_auth", os.path.join(DEV, "hermes_cli", "auth.py"))
auth = ilu.module_from_spec(spec)
sys.modules["wc_live_auth"] = auth
spec.loader.exec_module(auth)

NOUS_SUB_MSG = "Check credits or billing in Nous Portal, then retry."


def trace(code, provider, relogin, msg):
    c = Cup("format_auth_error")
    if code == "-":
        err = RuntimeError(msg)                     # non-AuthError -> B1
    else:
        err = auth.AuthError(
            msg, provider=provider or "", code=code,
            relogin_required=bool(relogin))

    if not isinstance(err, auth.AuthError):         # B1
        c.branch(True)
        out = str(err)
        c.valstr(out)
        return c.finalize()
    c.branch(False)
    if auth.is_rate_limited_auth_error(err):        # B2
        c.branch(True)
        out = str(err)
        c.valstr(out)
        return c.finalize()
    c.branch(False)
    if err.relogin_required:                        # B3
        c.branch(True)
        out = f"{err} Run `hermes model` to re-authenticate."
        c.valstr(out)
        return c.finalize()
    c.branch(False)
    nous = (err.provider or "") == "nous"
    if err.code == "subscription_required":         # B4
        c.branch(True)
        c.branch(nous)                              # B5
        out = NOUS_SUB_MSG if nous else (
            "No active paid subscription found. Please purchase/activate "
            "a subscription, then retry.")
        c.valstr(out)
        return c.finalize()
    c.branch(False)
    if err.code == "insufficient_credits":          # B6
        c.branch(True)
        c.branch(nous)                              # B7
        out = NOUS_SUB_MSG if nous else (
            "Subscription credits are exhausted. Top up/renew credits, "
            "then retry.")
        c.valstr(out)
        return c.finalize()
    c.branch(False)
    if err.code == "temporarily_unavailable":       # B8
        c.branch(True)
        out = f"{err} Please retry in a few seconds."
        c.valstr(out)
        return c.finalize()
    c.branch(False)
    c.valstr(str(err))
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        code, rest = raw.split("|", 1)
        provider, rest = rest.split("|", 1)
        relogin_s, msg = rest.split("|", 1)
        cups.append(trace(code, provider, int(relogin_s), msg))
    return cups
