#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_live_gates.py
(gate w22_telegramli_e4312e).

The module is two lines of policy over one shared helper, so the seams are:

  keys     -- live `_LIVE_GATE_EXTRA_KEYS`.
  allowlist-- live `env_allowlist_decision` with `platform_gate_env` bound to the
              fixture's TELEGRAM_ALLOWED_USERS value (the env read is the seam).
  refresh  -- live `gateway.platforms._shared.refresh_live_gate_extra`'s
              pre-reload decision: `gate_config_version` is bound to the fixture
              pair and `_reload_live_gate_extra` is stubbed True (the config load
              is the seam). That is exactly the branch `refresh_telegram_gates`
              takes before it touches the adapter.
  merge    -- live `_reload_live_gate_extra` with a fake `gateway.config`
              (config loader seam): the gate copy loop runs live against the
              fixture's loaded platform extras.
"""
import importlib.util
import json
import sys
import types
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "telegram" / "telegram_live_gates.py"
GATE = "w22_telegramli_e4312e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("tg_live_gates", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["tg_live_gates"] = live
_spec.loader.exec_module(live)

import gateway.platforms._shared as shared  # noqa: E402

WANTED = {"keys", "allowlist", "refresh", "merge"}


@contextmanager
def patched(obj, name, value):
    original = getattr(obj, name)
    setattr(obj, name, value)
    try:
        yield
    finally:
        setattr(obj, name, original)


def as_env(value):
    """Fixture env value -> what `platform_gate_env` would hand the module."""
    return value if isinstance(value, str) else ""


def allowlist(user_id, env):
    with patched(live, "platform_gate_env", lambda _name, default="": as_env(env)):
        return live.env_allowlist_decision(user_id)


class FakeAdapter:
    def __init__(self, platform, extra, baseline):
        self.platform = platform
        # A live adapter's `config.extra` is always a dict; a non-dict fixture
        # value stands for "no extras", which is what the C side starts from.
        self.config = types.SimpleNamespace(extra=dict(extra) if isinstance(extra, dict) else {})
        self._live_gate_version = baseline


class FakePlatformConfig:
    def __init__(self):
        self.extra = {}


class FakeLoaded:
    def __init__(self, platforms):
        self.platforms = platforms


def record(rec):
    """Fixture platform record -> the object `loaded.platforms.get(...)` returns."""
    if not rec:
        return None
    platform = FakePlatformConfig()
    if isinstance(rec, dict) and isinstance(rec.get("extra"), dict):
        platform.extra = dict(rec["extra"])
    return platform


def refresh(home, baseline_tag, current_tag):
    baseline = None
    if home is not None or baseline_tag is not None:
        baseline = (home if home is not None else "", baseline_tag, None)
    adapter = FakeAdapter("telegram", {}, baseline)
    version = (home if home is not None else "", current_tag, None)
    with patched(shared, "gate_config_version", lambda _home=None: version), \
         patched(shared, "_reload_live_gate_extra",
                 lambda _a, _k, _v, _cb, launch=False: True):
        return 1 if shared.refresh_live_gate_extra(adapter, list(live._LIVE_GATE_EXTRA_KEYS)) else 0


def merge(extra, platforms, platform):
    loaded = FakeLoaded({k: record(v) for k, v in (platforms or {}).items()})
    adapter = FakeAdapter(platform, extra, ("h", "v", None))
    fake = types.ModuleType("gateway.config")
    fake.PlatformConfig = FakePlatformConfig
    fake.load_gateway_config = lambda strict=True: loaded
    saved = sys.modules.get("gateway.config")
    sys.modules["gateway.config"] = fake
    try:
        shared._reload_live_gate_extra(adapter, list(live._LIVE_GATE_EXTRA_KEYS),
                                       ("h", "v", None), None)
        return dict(adapter.config.extra)
    finally:
        if saved is None:
            sys.modules.pop("gateway.config", None)
        else:
            sys.modules["gateway.config"] = saved


def expected(r):
    f = r.get("func")
    if f == "keys":
        return list(live._LIVE_GATE_EXTRA_KEYS)
    if f == "allowlist":
        return allowlist(r.get("user_id"), r.get("env"))
    if f == "refresh":
        return refresh(r.get("baseline_home"), r.get("baseline_version"), r.get("current_version"))
    if f == "merge":
        return merge(r.get("extra"), r.get("platforms"), r.get("platform"))
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
