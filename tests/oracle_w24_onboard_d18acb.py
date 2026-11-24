#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/qqbot/onboard.py
(gate w24_onboard_d18acb).

The live module is imported and its constants/enum are the ones compared
against.  The portal responses, the QR renderer, httpx and time are seams: each
function below is the live function's value half over a response body handed in
as the case.

  * ``build_connect_url`` is called live (quote + template);
  * ``_poll_bind_result``'s parse is re-derived with the live ``BindStatus`` enum
    so an out-of-range or non-int status is the same ValueError;
  * ``_portal_post``'s retcode check and ``_create_bind_task``'s missing-task_id
    check are re-derived with the live exception messages;
  * ``qr_register``'s expiry decision uses the live ``_MAX_REFRESHES``.

A live exception is the seam's error answer: the C port returns null there, so
the expected value is None.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_onboard_d18acb"

_HOME = REPO.parent
for _key, _val in (("USERPROFILE", str(_HOME)),
                   ("LOCALAPPDATA", str(_HOME / "AppData" / "Local")),
                   ("APPDATA", str(_HOME / "AppData" / "Roaming"))):
    if not os.environ.get(_key):
        os.environ[_key] = _val

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.qqbot import onboard as live  # noqa: E402

WANTED = {"url", "poll_result", "portal_error", "create_result", "expiry",
          "status_name", "constants"}


def url(r):
    return live.build_connect_url(r.get("task_id"))


def poll_result(r):
    data = r.get("data")
    d = data.get("data", {})
    status = live.BindStatus(d.get("status", 0))
    return {"status": int(status), "status_name": status.name,
            "bot_appid": str(d.get("bot_appid", "")),
            "bot_encrypt_secret": d.get("bot_encrypt_secret", ""),
            "user_openid": d.get("user_openid", "")}


def portal_error(r):
    data = r.get("data")
    fail_msg = r.get("fail_msg")
    if data.get("retcode") != 0:
        try:
            raise RuntimeError(data.get("msg", fail_msg))
        except RuntimeError as exc:
            return {"error": exc.args[0] if exc.args else None}
    return None


def create_result(r):
    data = r.get("data")
    task_id = (data.get("data") or {}).get("task_id")
    if not task_id:
        return {"error": "create_bind_task: missing task_id in response"}
    return {"task_id": task_id}


def expiry(r):
    refresh_count = r.get("refresh_count")
    if refresh_count >= live._MAX_REFRESHES:
        return {"action": "give_up"}
    return {"action": "refresh", "refresh_number": refresh_count + 1}


def status_name(r):
    return live.BindStatus(r.get("status")).name


def constants():
    return {
        "api_timeout": float(live.ONBOARD_API_TIMEOUT),
        "poll_interval": float(live.ONBOARD_POLL_INTERVAL),
        "create_path": live.ONBOARD_CREATE_PATH,
        "poll_path": live.ONBOARD_POLL_PATH,
        "portal_host": live.PORTAL_HOST,
        "qr_url_template": live.QR_URL_TEMPLATE,
        "max_refreshes": live._MAX_REFRESHES,
        "max_tasks": live._MAX_REFRESHES + 1,
        "statuses": {m.name: int(m) for m in live.BindStatus},
    }


def expected(r):
    f = r.get("func")
    if f == "url":
        return url(r)
    if f == "poll_result":
        return poll_result(r)
    if f == "portal_error":
        return portal_error(r)
    if f == "create_result":
        return create_result(r)
    if f == "expiry":
        return expiry(r)
    if f == "status_name":
        return status_name(r)
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
        except Exception:  # noqa: BLE001 - the live ValueError/AttributeError/TypeError seam
            py = None
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
