#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_outbound.py
(gate w24_telegramou_2e8580).

The live module is imported and its four pure functions are called directly:

  * ``is_visually_empty`` over text that is empty, whitespace, invisible code
    points, or a falsy non-text value;
  * ``notification_kwargs`` over the resolved mode and the caller's notify flag;
  * ``_metadata`` over a thread id and a notify flag;
  * ``resolve_notifications_mode`` with the two reads it performs (the
    ``HERMES_TELEGRAM_NOTIFICATIONS`` environment value and the config value)
    supplied per case: the env var is really set for the call and
    ``hermes_cli.config.cfg_get`` is patched to answer the config value, so the
    env-first / config-fallback / unknown-default branches all run in the live
    code.

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
GATE = "w24_telegramou_2e8580"

# The gate shell is a login shell with no USERPROFILE/LOCALAPPDATA exported, and
# the live import chain (agent.auxiliary_client -> hermes_constants) asks
# Path.home() for one. Hand it a deterministic home first.
_HOME = REPO.parent
for _key, _val in (("USERPROFILE", str(_HOME)),
                   ("LOCALAPPDATA", str(_HOME / "AppData" / "Local")),
                   ("APPDATA", str(_HOME / "AppData" / "Roaming"))):
    if not os.environ.get(_key):
        os.environ[_key] = _val

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.telegram import telegram_outbound as live  # noqa: E402
import hermes_cli.config as hcfg  # noqa: E402

ENV_VAR = "HERMES_TELEGRAM_NOTIFICATIONS"
WANTED = {"is_visually_empty", "notification_kwargs", "metadata", "resolve_mode", "constants"}


def is_visually_empty(r):
    return live.is_visually_empty(r.get("text"))


def notification_kwargs(r):
    return live.notification_kwargs(r.get("mode"), r.get("notify"))


def metadata(r):
    return live._metadata(r.get("thread_id"), r.get("notify"))


def resolve_mode(r):
    env = r.get("env")
    if env is None:
        os.environ.pop(ENV_VAR, None)
    else:
        os.environ[ENV_VAR] = env if isinstance(env, str) else str(env)
    cfg_raw = r.get("cfg")
    hcfg.cfg_get = lambda *a, **k: cfg_raw
    hcfg.load_config_readonly = lambda *a, **k: {}
    try:
        return live.resolve_notifications_mode()
    finally:
        os.environ.pop(ENV_VAR, None)


def constants():
    return {"notification_modes": list(live._NOTIFICATION_MODES),
            "invisible_codepoints": [int(k) for k in live._INVISIBLE]}


def expected(r):
    f = r.get("func")
    if f == "is_visually_empty":
        return is_visually_empty(r)
    if f == "notification_kwargs":
        return notification_kwargs(r)
    if f == "metadata":
        return metadata(r)
    if f == "resolve_mode":
        return resolve_mode(r)
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
        except Exception:  # noqa: BLE001 - the live AttributeError/TypeError seam
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
