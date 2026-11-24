#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/signal/register.py (gate w25_register_bdae35).

The live module is imported (its absolute imports resolve against the quarry)
and driven through its own entry points:

  * ``_parse_send_target`` -- the target-ref parser, NOT_EXPLICIT included;
  * ``_is_connected`` -- with ``hermes_cli.gateway.get_env_value`` and the
    scoped-secret reader pinned to the fixture's env;
  * ``register(fake_ctx)`` -- the real PlatformEntry kwargs are captured, not
    re-typed, so the constant table is compared against what the source passes.
"""
import importlib
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_register_bdae35"

# The quarry's import chain touches the profile home; a login shell may have no
# HOME, so pin one before anything is imported.
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("plugins.platforms.signal.register")  # noqa: E402
sigad = importlib.import_module("plugins.platforms.signal.adapter")  # noqa: E402
gateway = importlib.import_module("hermes_cli.gateway")  # noqa: E402
send_targets = importlib.import_module("gateway.platforms.send_targets")  # noqa: E402

WANTED = {"parse_send_target", "is_connected", "platform_hint", "register_spec"}


class _Ctx:
    def __init__(self):
        self.kw = None

    def register_platform(self, **kw):
        self.kw = kw


def parse_send_target(req):
    got = live._parse_send_target(req.get("ref"))
    if got is send_targets.NOT_EXPLICIT:
        return "NOT_EXPLICIT"
    if isinstance(got, tuple):
        return list(got)
    return got


def _env_of(req):
    env = req.get("env")
    return env if isinstance(env, dict) else {}


def _pin_env(env):
    saved = (gateway.get_env_value, sigad._sig_secret)
    gateway.get_env_value = lambda name, default=None: (
        env.get(name, "" if default is None else default))
    # The scoped-secret reader is the same source: the fixture env is os.environ.
    sigad._sig_secret = lambda name, default=None, **kw: env.get(name, default)
    return saved


def _unpin_env(saved):
    gateway.get_env_value, sigad._sig_secret = saved


def is_connected(req):
    extra = req.get("extra")
    cfg = types.SimpleNamespace(extra=extra if isinstance(extra, dict) else None)
    saved = _pin_env(_env_of(req))
    try:
        return bool(live._is_connected(cfg))
    finally:
        _unpin_env(saved)


def platform_hint(_req):
    return live._PLATFORM_HINT


def register_spec(_req):
    ctx = _Ctx()
    live.register(ctx)
    kw = ctx.kw
    policy = kw["standalone_send"]
    return {
        "name": kw["name"],
        "label": kw["label"],
        "dashboard_description": kw["dashboard_description"],
        "docs_url": kw["docs_url"],
        "dashboard_env_vars": list(kw["dashboard_env_vars"]),
        "required_env": list(kw["required_env"]),
        "cron_deliver_env_var": kw["cron_deliver_env_var"],
        "install_hint": kw["install_hint"],
        "emoji": kw["emoji"],
        "pii_safe": bool(kw["pii_safe"]),
        "allow_update_command": bool(kw["allow_update_command"]),
        "display_tier": kw["display_tier"],
        "max_message_length": float(kw["max_message_length"]),
        "platform_hint": kw["platform_hint"],
        "standalone_send": {
            "label": policy.label,
            "media": bool(policy.media),
            "all_sends": bool(policy.all_sends),
            "captionable": bool(policy.captionable),
            "empty_list_between_chunks": bool(policy.empty_list_between_chunks),
            "forward_force_document": bool(policy.forward_force_document),
            "mentions": bool(policy.mentions),
            "text_direct": bool(policy.text_direct),
            "reply_file_over": float(policy.reply_file_over),
        },
        "allowed_users_env": kw["allowed_users_env"],
        "allow_all_env": kw["allow_all_env"],
        "allowlist_suppresses_pairing": bool(kw["allowlist_suppresses_pairing"]),
    }


def expected(r):
    f = r.get("func")
    if f == "parse_send_target":
        return parse_send_target(r)
    if f == "is_connected":
        return is_connected(r)
    if f == "platform_hint":
        return platform_hint(r)
    if f == "register_spec":
        return register_spec(r)
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
