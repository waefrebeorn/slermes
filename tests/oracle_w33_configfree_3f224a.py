#!/usr/bin/env python3
"""Differential oracle for tui_gateway/contracts/config_free_tier_control.py
(gate w33_configfree_3f224a).

The contracts module names the wire shape of the free-tier control surface; the
values behind it are decided by five pure bodies, all driven live here:

  * ``tui_gateway.methods_free_tier``'s ``free_tier.status`` handler, rebound
    onto a stub namespace with ``_ok`` / ``_err`` (the one seam ``bind_module``
    supplies) -- its payload assembly is exactly what the port must reproduce;
  * ``hermes_cli.anon_auth``: is_guest_state / free_inference_active /
    guest_without_free_tier / guest_notice_pending / anon_failure_copy /
    MintFailure.as_payload;
  * ``hermes_cli.anon_challenge``: parse_browser_challenge / pending_challenge /
    record_host_outcome (the per-profile record and the clock are injected);
  * ``hermes_cli.welcome_trial``: claim_blocker (the install probe, the guest
    switch and the identity/provider checks are injected).

Every seam is a fixture value: no auth store is read, no mint runs, no file or
clock is touched.  ``str(QUARRY)`` sits at sys.path[0] so this repo's
tests/hermes_cli and tests/tools cannot shadow the quarry.
"""
import json
import sys
import time
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_configfree_3f224a"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(REPO / "tests"))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

import tui_gateway.methods_free_tier as methods_free_tier  # noqa: E402
from tui_gateway.method_ctx import rebind  # noqa: E402
from hermes_cli import anon_auth, anon_challenge, free_tier_offer, install_state, welcome_trial  # noqa: E402

WANTED = {
    "status_payload", "guest_state", "free_inference", "guest_without_tier",
    "notice_pending", "failure_copy", "mint_payload", "challenge_parse",
    "challenge_payload", "challenge_outcome", "claim_blocker",
}


# --- the handler seam: server.py's _ok/_err (bind_module's only obligation) ---
def _ok(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


def _err(rid, code, msg, data=None):
    err = {"code": code, "message": msg}
    if data is not None:
        err["data"] = data
    return {"jsonrpc": "2.0", "id": rid, "error": err}


_GLOBALS = {"_ok": _ok, "_err": _err, "__name__": "w33_configfree_3f224a_seam",
            "_trial_claimable": methods_free_tier._trial_claimable}
_STATUS = []

# The status driver patches module-level seams; the other drivers must see the
# live functions again, so the originals are latched here.
_ORIGINAL = {
    "pending_challenge": anon_challenge.pending_challenge,
    "claim_blocker": welcome_trial.claim_blocker,
}


def restore(*names):
    for name in names:
        owner, attr = {
            "pending_challenge": (anon_challenge, "pending_challenge"),
            "claim_blocker": (welcome_trial, "claim_blocker"),
        }[name]
        setattr(owner, attr, _ORIGINAL[name])


def status_handler():
    if not _STATUS:
        pending = dict(methods_free_tier._registry._pending)
        _STATUS.append(rebind(pending["free_tier.status"], _GLOBALS))
    return _STATUS[0]


# --- the clock seam (time.monotonic inside anon_auth / anon_challenge) --------
_clock_target = []


class _Clock:
    def __init__(self, now):
        self._now = float(now)

    def monotonic(self):
        return self._now

    def __getattr__(self, name):
        return getattr(time, name)


def set_clock(module, now):
    module.time = _Clock(now)
    _clock_target.append(module)


# --- live drivers -------------------------------------------------------------
def status_payload(req):
    ident = req.get("identity")
    ident = ident if isinstance(ident, dict) else {}
    state = ident.get("state")
    anon_auth.current_nous_state = lambda: state
    anon_auth.guest_allowed = lambda: bool(ident.get("guest_allowed"))
    welcome_trial.claim_blocker = lambda **kw: ident.get("trial_blocker")
    free_tier_offer.offer_due_in = lambda kind=None: (
        ident.get("connector_nudge_due_in") if kind == free_tier_offer.CONNECTOR
        else ident.get("nudge_due_in"))
    anon_auth.last_mint_failure = lambda: ident.get("mint_failure")
    anon_challenge.pending_challenge = lambda: ident.get("challenge")
    out = status_handler()(1, {"profile": None})
    if "error" in out:
        raise AssertionError("free_tier.status answered an error: %r" % (out["error"],))
    return out["result"]


def guest_state(req):
    return anon_auth.is_guest_state(req.get("state"))


def _inject_state(req):
    state = req.get("state")
    anon_auth.current_nous_state = lambda: state
    anon_auth.guest_allowed = lambda: bool(req.get("guest_allowed"))
    return state


def free_inference(req):
    return anon_auth.free_inference_active(_inject_state(req))


def guest_without_tier(req):
    return anon_auth.guest_without_free_tier(_inject_state(req))


def notice_pending(req):
    _inject_state(req)
    return anon_auth.guest_notice_pending()


def failure_copy(req):
    return anon_auth.anon_failure_copy(req.get("code"), retry_after=req.get("retry_after"))


def mint_payload(req):
    failure = req.get("failure")
    if not isinstance(failure, dict):
        return None
    set_clock(anon_auth, req.get("now") or 0)
    mint = anon_auth.MintFailure(
        code=failure.get("code", ""),
        message=failure.get("message", ""),
        retryable=bool(failure.get("retryable")),
        retry_after=float(failure.get("retry_after") or 0),
        not_before=float(failure.get("not_before") or 0),
    )
    return mint.as_payload()


def _challenge(record):
    if not record:
        return None
    return {
        "url": record.get("url", ""),
        "required": bool(record.get("required")),
        "expires_in": float(record.get("expires_in") or 0),
        "interval": float(record.get("interval") or 0),
        "message": record.get("message", ""),
        "attempt": int(record.get("attempt") or 0),
    }


def challenge_parse(req):
    payload = req.get("payload")
    if not isinstance(payload, dict):
        return None
    found = anon_challenge.parse_browser_challenge(payload, req.get("portal") or "")
    if found is None:
        return None
    return {
        "url": found.url,
        "required": found.required,
        "expires_in": found.expires_in,
        "interval": found.interval,
        "message": found.message,
        "attempt": found.attempt,
    }


def _record(req):
    rec = req.get("rec")
    rec = rec if isinstance(rec, dict) else {}
    ch = _challenge(rec.get("pending"))
    challenge = None
    if ch is not None:
        challenge = anon_challenge.BrowserChallenge(
            url=ch["url"], required=ch["required"], expires_in=ch["expires_in"],
            interval=ch["interval"], message=ch["message"], attempt=ch["attempt"])
    record = anon_challenge._ProfileChallenge()
    record.pending = challenge
    record.deadline = float(rec.get("deadline") or 0)
    record.outcome = rec.get("outcome")
    return record


def challenge_payload(req):
    restore("pending_challenge")
    record = _record(req)
    anon_challenge._profile = lambda: record
    set_clock(anon_challenge, req.get("now") or 0)
    return anon_challenge.pending_challenge()


def challenge_outcome(req):
    record = _record(req)
    anon_challenge._profile = lambda: record
    set_clock(anon_challenge, req.get("now") or 0)
    return anon_challenge.record_host_outcome(
        req.get("url"), int(req.get("attempt") or 0), "done")


def claim_blocker(req):
    restore("claim_blocker")
    ask = req.get("ask")
    ask = ask if isinstance(ask, dict) else {}
    anon_auth.on_local_desktop_backend = lambda: bool(ask.get("on_local_desktop_backend"))
    anon_auth.guest_allowed = lambda: bool(ask.get("guest_allowed"))
    welcome_trial._has_nous_identity = lambda: bool(ask.get("has_nous_identity"))
    welcome_trial.provider_configured = lambda: bool(ask.get("provider_configured"))
    install_state.probe = lambda: types.SimpleNamespace(existing=bool(ask.get("machine_used")))
    install_state.trial_reserved = lambda: bool(ask.get("trial_reserved"))
    return welcome_trial.claim_blocker(
        assume_fresh=bool(ask.get("assume_fresh")), configured=ask.get("configured"))


EXPECTED = {
    "status_payload": status_payload,
    "guest_state": guest_state,
    "free_inference": free_inference,
    "guest_without_tier": guest_without_tier,
    "notice_pending": notice_pending,
    "failure_copy": failure_copy,
    "mint_payload": mint_payload,
    "challenge_parse": challenge_parse,
    "challenge_payload": challenge_payload,
    "challenge_outcome": challenge_outcome,
    "claim_blocker": claim_blocker,
}


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
            py = EXPECTED[r["func"]](r)
        except Exception as exc:  # noqa: BLE001 - a broken case is a mismatch, not a crash
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
