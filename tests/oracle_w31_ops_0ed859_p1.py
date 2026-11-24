#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/ops.py — gate w31_ops_0ed859_p1
(webhook payload shaping).

The live router module is imported BY FILE PATH (its FastAPI decorators and the
pydantic models would otherwise drag the whole web_server graph in, and this
host refuses pydantic's native extension).  The pure helpers are called
directly:

  * ``_webhook_route_summary`` over a subscription record;
  * ``_dashboard_route_signature`` over a create-body pin + the stored record;
  * ``_webhook_key_in`` over a lock-held subscription snapshot.

Two guards live INSIDE ``create_webhook`` rather than in a helper (the name slug
and the deliver_only/"log" refusal).  They are transcribed here verbatim from
that function -- ``name = (body.name or "").strip().lower().replace(" ", "-")``
+ ``re.match(r"^[a-z0-9][a-z0-9_-]*$", name)`` and
``if body.deliver_only and body.deliver == "log"`` -- because there is no
seam-free way to call the route itself (it awaits config scopes and the
webhook store).

Seams, all supplied as data here: the webhook JSON store (``subscriptions`` is
a fixture object), the base URL, and the platform-enabled read.  A refusal is
the ``HTTPException`` the route would have raised, projected as
``{"ok": false, "status": int, "detail": str}``.
"""
import importlib.util
import json
import os
import re
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_ops_0ed859_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

if os.name == "nt" and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)

sys.path.append(str(Path(__file__).resolve().parent))  # stub helper only; must NOT shadow the quarry packages

from hermes_seam_stubs import install as _install_seams  # noqa: E402

HTTPException = _install_seams()


def _load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY / rel)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


live = _load("w31_live_ops_p1", "hermes_cli/web_routers/ops.py")

WANTED = {
    "webhook_route_summary", "dashboard_route_signature", "webhook_key_in",
    "webhook_name_verdict", "webhook_deliver_verdict",
}

NAME_RE = re.compile(r"^[a-z0-9][a-z0-9_-]*$")
NAME_DETAIL = "Invalid name. Use lowercase alphanumeric with hyphens/underscores."
DELIVER_DETAIL = (
    "Direct delivery requires a real target (telegram, discord, \u2026), not 'log'."
)


def ok(**extra):
    out = {"ok": True, "status": 200, "detail": None}
    out.update(extra)
    return out


def refused(status, detail):
    return {"ok": False, "status": status, "detail": detail}


def route_summary(r):
    return live._webhook_route_summary(
        r.get("name") or "", r.get("route") or {}, r.get("base_url") or "",
    )


def dashboard_route_signature(r):
    return live._dashboard_route_signature(r.get("requested"), r.get("expected"))


def webhook_key_in(r):
    try:
        key = live._webhook_key_in(r.get("subscriptions") or {}, r.get("name") or "")
    except HTTPException as exc:
        return refused(exc.status_code, exc.detail)
    return ok(key=key)


def webhook_name_verdict(r):
    # create_webhook, verbatim:
    name = (r.get("name") or "").strip().lower().replace(" ", "-")
    if not NAME_RE.match(name):
        return refused(400, NAME_DETAIL)
    return ok(name=name)


def webhook_deliver_verdict(r):
    # create_webhook, verbatim: deliver defaults to "log" on the model.
    deliver = r["deliver"] if "deliver" in r else "log"
    if r.get("deliver_only") and deliver == "log":
        return refused(400, DELIVER_DETAIL)
    return ok()


def expected(r):
    f = r.get("func")
    if f == "webhook_route_summary":
        return route_summary(r)
    if f == "dashboard_route_signature":
        return dashboard_route_signature(r)
    if f == "webhook_key_in":
        return webhook_key_in(r)
    if f == "webhook_name_verdict":
        return webhook_name_verdict(r)
    if f == "webhook_deliver_verdict":
        return webhook_deliver_verdict(r)
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
