#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/gateway_env.py
(gate w22_gatewayenv_1c1b2e).

The module declares, it does not execute: ``ENV_ENABLE_CREDENTIALS``,
``ENV_OVERRIDES`` and ``AUTHZ`` are read off the live module, and the override
steps are described by introspecting the live objects they are (``EnvCred``
instances, the ``functools.partial`` steps and the one plain function), so the
C descriptors are judged against the objects core would actually run.

``_fallback_ips`` is the only step with body logic.  `getenv` is imported into
the module namespace, so the env seam is that name -- the stub mirrors
``_getenv_str`` (text or the default; env values are never non-strings) -- and
a stub config records which PlatformConfig the step wrote to.
"""
import functools
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "telegram" / "gateway_env.py"
GATE = "w22_gatewayenv_1c1b2e"
ENV_NAME = "TELEGRAM_FALLBACK_IPS"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("telegram_gateway_env_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["telegram_gateway_env_live"] = live
_spec.loader.exec_module(live)

WANTED = {"enable", "authz", "overrides", "fallback"}


def describe(step):
    """The wire descriptor for one ENV_OVERRIDES step."""
    if hasattr(step, "creds"):
        return {
            "kind": "cred",
            "platform": step.platform.value,
            # each group is a bare env name or an inner tuple of alternatives
            "creds": [[g] if isinstance(g, str) else list(g) for g in step.creds],
            "token": step.token,
        }
    if isinstance(step, functools.partial):
        name = step.func.__name__
        kw = step.keywords
        if name == "env_reply_mode":
            return {"kind": "reply_mode", "platform": kw["platform"].value, "env": kw["env"]}
        if name == "env_home_channel":
            return {"kind": "home", "platform": kw["platform"].value,
                    "env_base": kw["env_base"], "strip": bool(kw.get("strip"))}
        raise AssertionError("unexpected partial step %r" % (name,))
    return {"kind": "callable", "name": step.__name__}


class StubConfig:
    def __init__(self):
        self.platforms = {}


def fallback(env):
    values = env if isinstance(env, dict) else {}
    original = live.getenv

    def getenv(name, default=""):
        value = values.get(name)
        return value if isinstance(value, str) else default

    live.getenv = getenv
    try:
        config = StubConfig()
        live._fallback_ips(config)
    finally:
        live.getenv = original
    entry = config.platforms.get(live.Platform.TELEGRAM)
    if entry is None:
        return None
    return {"fallback_ips": entry.extra.get("fallback_ips")}


def expected(r):
    f = r.get("func")
    if f == "enable":
        return list(live.ENV_ENABLE_CREDENTIALS)
    if f == "authz":
        return dict(live.AUTHZ)
    if f == "overrides":
        return [describe(step) for step in live.ENV_OVERRIDES]
    if f == "fallback":
        return fallback(r.get("env"))
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
