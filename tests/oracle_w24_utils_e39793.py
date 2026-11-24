#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/qqbot/utils.py
(gate w24_utils_e39793).

The live module is imported whole and kept live: ``coerce_list`` is called
directly on the fixture value, and for the User-Agent the three host-fact seams
it reads (``sys.version_info``, ``platform.system()``, ``_get_hermes_version``)
plus ``QQBOT_VERSION`` are swapped for the fixture's values *in the module's own
namespace* and restored afterwards -- so the f-string is upstream's.
``get_api_headers`` runs with ``build_user_agent`` stubbed to the fixture UA.

``platform.system`` is lower-cased by the live code; the C port does the same.
"""
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_utils_e39793"

# git-bash login shells carry no USERPROFILE, which Path.home() needs.
if not (os.environ.get("USERPROFILE") or os.environ.get("HOMEDRIVE")):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.qqbot import utils as live              # noqa: E402
from plugins.platforms.qqbot import constants as qconst        # noqa: E402

WANTED = {"coerce_list", "build_user_agent", "api_headers", "constants"}

SEAM_ERRORS = (TypeError, ValueError, AttributeError)


def coerce_list(r):
    return live.coerce_list(r.get("value"))


def build_user_agent(r):
    saved = (live.sys, live.platform, live._get_hermes_version, live.QQBOT_VERSION)
    try:
        live.sys = types.SimpleNamespace(
            version_info=types.SimpleNamespace(
                major=r.get("py_major", 0), minor=r.get("py_minor", 0),
                micro=r.get("py_micro", 0)))
        live.platform = types.SimpleNamespace(system=lambda: r.get("os_name", ""))
        live._get_hermes_version = lambda: r.get("hermes_version", "")
        if "qqbot_version" in r:
            live.QQBOT_VERSION = r["qqbot_version"]
        return live.build_user_agent()
    finally:
        live.sys, live.platform, live._get_hermes_version, live.QQBOT_VERSION = saved


def api_headers(r):
    saved = live.build_user_agent
    try:
        ua = r.get("user_agent", "")
        live.build_user_agent = lambda: ua
        return live.get_api_headers()
    finally:
        live.build_user_agent = saved


def constants():
    headers = live.get_api_headers()
    return {
        "qqbot_version": qconst.QQBOT_VERSION,
        "api_base": qconst.API_BASE,
        "token_url": qconst.TOKEN_URL,
        "content_type": headers["Content-Type"],
        "accept": headers["Accept"],
    }


def expected(r):
    f = r.get("func")
    if f == "coerce_list":
        return coerce_list(r)
    if f == "build_user_agent":
        return build_user_agent(r)
    if f == "api_headers":
        return api_headers(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = seams = 0
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
        except SEAM_ERRORS as exc:
            seams += 1
            if r.get("out") is not None:
                mism += 1
                print("MISMATCH %s case=%r live raises %r but c=%r (expected null)"
                      % (r.get("func"), r.get("case"), exc, r.get("out")), file=sys.stderr)
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
