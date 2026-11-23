#!/usr/bin/env python3
"""Differential oracle for w8_handofftarg_ecfd33 -> gateway/handoff_target.py.

gateway.config (Platform/HomeChannel/load_gateway_config) and gateway.relay
(relay_platform_identities) are registered as scripted stand-ins whose values
come from each fixture.  HandoffTargetError is the live exception class.
"""
import enum
import importlib.util
import json
import sys
import types

G = "w8_handofftarg_ecfd33"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class _PlatformVal:
    """Value-based stand-in for gateway.config.Platform; membership is the
    fixture's known_platforms list (the real enum's dynamic plugin members)."""

    def __init__(self, value):
        self.value = value

    def __eq__(self, other):
        return isinstance(other, _PlatformVal) and self.value == other.value

    def __hash__(self):
        return hash(self.value)

    def __str__(self):
        return self.value


class _PlatformFactory:
    def __call__(self, value):
        if not isinstance(value, str) or value not in known_holder[0]:
            raise ValueError(value)
        return _PlatformVal(value)


known_holder = [[]]
Platform = _PlatformFactory()
Platform.RELAY = _PlatformVal("relay")


class HomeChannel:
    def __init__(self, platform, chat_id, name=""):
        self.platform = platform
        self.chat_id = chat_id
        self.name = name


class _Native:
    def __init__(self, d):
        self.enabled = bool(d.get("enabled")) if isinstance(d, dict) else False


class _Cfg:
    def __init__(self, r):
        self.platforms = {_PlatformVal(k): _Native(v) for k, v in (r.get("platforms") or {}).items()}
        if r.get("relay_enabled"):
            self.platforms[Platform.RELAY] = _Native({"enabled": True})
        self._homes = {}
        for k, v in (r.get("homes") or {}).items():
            try:
                p = Platform(k)
            except ValueError:
                continue
            self._homes[p] = HomeChannel(p, v.get("chat_id") or "", v.get("name", ""))

    def get_home_channel(self, platform):
        return self._homes.get(platform)


config_holder = [None]
relay_holder = [[]]
_gw = _pkg("gateway")
_cfgmod = _mod("gateway.config", Platform=Platform, HomeChannel=HomeChannel,
               load_gateway_config=lambda: config_holder[0])
_gw.config = _cfgmod
_mod("gateway.relay",
     relay_platform_identities=lambda: [(n, None) for n in relay_holder[0]])

SRC = Q + "/gateway/handoff_target.py"
spec = importlib.util.spec_from_file_location("w8_handofftarg_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_handofftarg_live"] = live
spec.loader.exec_module(live)

def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    known_holder[0] = list(r.get("known_platforms") or [])
    cfg = _Cfg(r)
    config_holder[0] = cfg
    relay_holder[0] = list(r.get("relay_names") or [])
    platform_name = r.get("platform_name") or ""
    try:
        if func == "resolve":
            try:
                h = live.resolve_handoff_home(platform_name)
                py = {"ok": True, "value": {"chat_id": h.chat_id,
                                            "platform": h.platform.value}}
            except live.HandoffTargetError as exc:
                py = {"ok": False, "reason": exc.reason, "platform": exc.platform}
        elif func == "relay_fronts":
            py = live._relay_fronts(cfg, platform_name)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = canonical(r.get("out"))
    cases += 1
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={canonical(py)!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or not cases else 0)
