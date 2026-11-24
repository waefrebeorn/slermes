#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/whatsapp/gateway_env.py
(gate w22_gatewayenv_bc3450).

The module's steps mutate a live GatewayConfig, so the config object is the
seam: a recording stand-in carries `platforms`, and `getenv` /
`enable_from_env` are stubbed at the module boundary (both the whatsapp module's
own `getenv` and `gateway.platform_env.getenv`, which `home_step`'s partial
resolves through). Everything else runs live:

  enabled_flag   -- live `_enabled_flag` over one WHATSAPP_ENABLED reading; the
                    answer is the action it took and the resulting `.enabled`.
  home_channel   -- live `ENV_OVERRIDES[1]`, the `home_step(Platform.WHATSAPP,
                    "WHATSAPP_HOME_CHANNEL")` partial, over one env reading.
  authz / creds  -- the live AUTHZ literal and ENV_ENABLE_CREDENTIALS tuple.

`getenv` returning None is read as the name's default, which is exactly how the
C harness reads a present-but-null fixture key.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "whatsapp" / "gateway_env.py"
GATE = "w22_gatewayenv_bc3450"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("whatsapp_gateway_env_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["whatsapp_gateway_env_live"] = live
_spec.loader.exec_module(live)

import gateway.platform_env as platform_env  # noqa: E402  (same object `home_step` closes over)
from gateway.config import Platform  # noqa: E402

WANTED = {"env_enable_credentials", "authz", "enabled_flag", "home_channel"}

ENVS = {}
EVENTS = []


def fake_getenv(name, default=""):
    value = ENVS.get(name)
    # An env read is always a str; a non-str fixture value reads as absent, which
    # is exactly what the C harness's json_get_str default does.
    if not isinstance(value, str):
        return default
    return value


def fake_enable(config, platform):
    EVENTS.append("enable")
    config.platforms.setdefault(platform, FakePlatformCfg(False)).enabled = True


live.getenv = fake_getenv
platform_env.getenv = fake_getenv
live.enable_from_env = fake_enable


class FakePlatformCfg:
    def __init__(self, enabled):
        self.enabled = enabled
        self.home_channel = None


class FakeConfig:
    def __init__(self, has_platform, enabled):
        self.platforms = {}
        if has_platform:
            self.platforms[Platform.WHATSAPP] = FakePlatformCfg(enabled)


def env_enable_credentials():
    return list(live.ENV_ENABLE_CREDENTIALS)


def authz():
    table = dict(live.AUTHZ)
    table["own_policy_env"] = list(table["own_policy_env"])
    return table


def enabled_flag(raw, has_platform, yaml_enabled):
    global EVENTS
    EVENTS = []
    ENVS.clear()
    ENVS["WHATSAPP_ENABLED"] = "" if raw is None else raw
    config = FakeConfig(bool(has_platform), bool(yaml_enabled))
    live._enabled_flag(config)
    entry = config.platforms.get(Platform.WHATSAPP)
    if EVENTS:
        action = "enable"
    elif has_platform and yaml_enabled and entry is not None and entry.enabled is False:
        action = "disable"
    else:
        action = "none"
    return {"action": action, "enabled": None if entry is None else bool(entry.enabled)}


def home_channel(chat_id, name, thread_id, configured):
    ENVS.clear()
    ENVS["WHATSAPP_HOME_CHANNEL"] = chat_id
    ENVS["WHATSAPP_HOME_CHANNEL_NAME"] = name
    ENVS["WHATSAPP_HOME_CHANNEL_THREAD_ID"] = thread_id
    config = FakeConfig(bool(configured), True)
    live.ENV_OVERRIDES[1](config)
    entry = config.platforms.get(Platform.WHATSAPP)
    channel = entry.home_channel if entry is not None else None
    if channel is None:
        return None
    return {"platform": channel.platform.value, "chat_id": channel.chat_id,
            "name": channel.name, "thread_id": channel.thread_id}


def expected(r):
    f = r.get("func")
    if f == "env_enable_credentials":
        return env_enable_credentials()
    if f == "authz":
        return authz()
    if f == "enabled_flag":
        return enabled_flag(r.get("raw"), r.get("has_platform"), r.get("yaml_enabled"))
    if f == "home_channel":
        return home_channel(r.get("chat_id"), r.get("name"), r.get("thread_id"),
                            r.get("configured"))
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
