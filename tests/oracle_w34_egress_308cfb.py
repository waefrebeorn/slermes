#!/usr/bin/env python3
"""Differential oracle for gateway/relay/egress.py (gate w34_egress_308cfb).

The live module is imported as a package module from the quarry.  Every runtime
seam it reads lazily -- ``gateway.config`` (Platform + load_gateway_config),
``gateway.run._gateway_runner_ref`` (the adapter registry) and
``gateway.channel_directory`` (directory + session origins), plus the relay
package's ``relay_fronted_platforms`` -- is replaced per case by a stand-in
built from the fixture's own seam values.  Nothing connects, reads a real config
file, or starts a runner: what is compared is exactly what the values decide.

  * the decline classifiers (``is_egress_decline`` / ``declined_send`` /
    ``decline_error``) and the message ``log_decline`` logs;
  * ``_is_missing_gateway_relay`` (absence vs fault);
  * ``_has_live_native_adapter`` / ``relay_routed_platform`` (routing), where a
    RelayRouteUnknown fault is reported as the string "unknown";
  * ``_session_ids`` / ``attested_relay_targets`` (provenance);
  * ``_is_unresolved_handle`` / ``authorize_relay_target`` (the guard).
"""
import json
import logging
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_egress_308cfb"

# The quarry must win over this repo's tests/ tree (tests/gateway, tests/tools,
# tests/hermes_cli shadow the upstream packages).
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from gateway.relay import egress as live  # noqa: E402

WANTED = {
    "is_egress_decline",
    "declined_send",
    "decline_error",
    "decline_log_line",
    "is_missing_gateway_relay",
    "has_live_native_adapter",
    "relay_routed_platform",
    "session_ids",
    "attested_relay_targets",
    "is_unresolved_handle",
    "authorize_relay_target",
}


# ---------------------------------------------------------------------------
# seam stand-ins
# ---------------------------------------------------------------------------

class FakePlatform:
    """The config seam's Platform: constructs from any name, compares by value."""

    RELAY = "relay"

    def __init__(self, name):
        self.value = str(name)

    def __str__(self):
        return self.value

    def __hash__(self):
        return hash(self.value)

    def __eq__(self, other):
        return isinstance(other, FakePlatform) and other.value == self.value

    def __repr__(self):
        return "Platform(%s)" % self.value


class PConf:
    def __init__(self, enabled=True, token=None):
        self.enabled = enabled
        self.token = token


class Home:
    def __init__(self, chat_id):
        self.chat_id = chat_id


class Platforms:
    def __init__(self, data):
        self._d = data

    def get(self, platform):
        raw = self._d.get(str(platform))
        if raw is None:
            return None
        return PConf(enabled=bool(raw.get("enabled", True)), token=raw.get("token"))


class Cfg:
    def __init__(self, platforms, sources):
        self.platforms = Platforms(platforms)
        self._sources = sources

    def get_home_channel(self, platform):
        src = self._sources.get(str(platform))
        if not isinstance(src, dict):
            return None
        home = src.get("home")
        if home is None:
            return None
        return Home(home)


class Adapters:
    """The runner's registry: get(platform) is None unless that adapter is live."""

    def __init__(self, platform, present):
        self._platform = platform
        self._present = present

    def get(self, platform):
        name = str(platform)
        if name == "relay":  # the relay adapter is never "live" here
            return None
        return object() if (self._present and name == self._platform) else None

    def __bool__(self):
        return True


class Runner:
    def __init__(self, adapters):
        self.adapters = adapters


def install(req):
    """Point the live module's lazy imports at this case's seam values."""
    platform = req.get("platform")
    pname = "" if platform is None else str(platform).strip().lower()
    raw_sources = req.get("sources")
    sources = raw_sources if isinstance(raw_sources, dict) else {}

    if req.get("func") == "session_ids":  # entries are this seam's input
        entries = req.get("entries")
        sources = {str(req.get("platform") or "probe"): {"sessions": entries or []}}

    platmap = {}
    native_config = req.get("native_config")
    if native_config is not None:
        platmap[pname] = {
            "enabled": bool(native_config.get("enabled", True))
            if isinstance(native_config, dict)
            else True
        }
    elif req.get("native_credential"):
        platmap[pname] = {"enabled": True}
    if req.get("native_credential"):
        for raw in platmap.values():
            raw["token"] = "tok"

    cfgmod = types.ModuleType("gateway.config")
    cfgmod.Platform = FakePlatform

    def _load_config():
        if req.get("config_error"):
            raise RuntimeError("gateway config unreadable")
        return Cfg(platmap, sources)

    cfgmod.load_gateway_config = _load_config
    sys.modules["gateway.config"] = cfgmod

    runmod = types.ModuleType("gateway.run")
    if req.get("route_unknown"):

        def _runner_ref():
            raise RuntimeError("runner registry unreadable")

    elif req.get("runner_present"):
        adapters = Adapters(pname, bool(req.get("adapter_present")))

        def _runner_ref():
            return Runner(adapters)

    else:

        def _runner_ref():
            return None

    runmod._gateway_runner_ref = _runner_ref
    sys.modules["gateway.run"] = runmod

    cdmod = types.ModuleType("gateway.channel_directory")

    def _load_directory():
        return {
            "platforms": {
                name: (src or {}).get("directory") or []
                for name, src in sources.items()
            }
        }

    def _from_sessions(name):
        src = sources.get(str(name)) or {}
        return src.get("sessions") or []

    cdmod.load_directory = _load_directory
    cdmod._build_from_sessions = _from_sessions
    sys.modules["gateway.channel_directory"] = cdmod

    import gateway.relay as relay_pkg  # noqa: PLC0415

    fronted = req.get("fronted")
    relay_pkg.relay_fronted_platforms = lambda: [
        str(x) for x in (fronted if isinstance(fronted, list) else [])
    ]


# ---------------------------------------------------------------------------
# one function per ported symbol
# ---------------------------------------------------------------------------

class SendResult:
    """A projected SendResult: only the attributes the fixture names exist."""

    def __init__(self, proj):
        if isinstance(proj, dict):
            if "raw_response" in proj:
                self.raw_response = proj["raw_response"]
            if "error" in proj:
                self.error = proj["error"]


class Capture(logging.Handler):
    def __init__(self):
        super().__init__()
        self.messages = []

    def emit(self, record):
        self.messages.append(record.getMessage())


def f_is_egress_decline(req):
    return bool(live.is_egress_decline(req.get("result")))


def f_declined_send(req):
    return bool(live.declined_send(SendResult(req.get("result"))))


def f_decline_error(req):
    return live.decline_error(req.get("result"))


def f_decline_log_line(req):
    handler = Capture()
    previous = live.logger.level
    live.logger.addHandler(handler)
    live.logger.setLevel(logging.DEBUG)
    try:
        live.log_decline(req.get("op"), req.get("chat_id"), req.get("result"))
    finally:
        live.logger.removeHandler(handler)
        live.logger.setLevel(previous)
    if len(handler.messages) != 1:
        raise AssertionError("log_decline emitted %d records" % len(handler.messages))
    return handler.messages[0]


def f_is_missing_gateway_relay(req):
    name = req.get("name")
    if req.get("module_not_found"):
        exc = ModuleNotFoundError("No module named %r" % (name,), name=name)
    else:
        exc = ImportError("cannot import name", name=name)
    return bool(live._is_missing_gateway_relay(exc))


def f_has_live_native_adapter(req):
    try:
        return "true" if live._has_live_native_adapter(req.get("platform")) else "false"
    except live.RelayRouteUnknown:
        return "unknown"


def f_relay_routed_platform(req):
    try:
        return "true" if live.relay_routed_platform(req.get("platform")) else "false"
    except live.RelayRouteUnknown:
        return "unknown"


def f_session_ids(req):
    return sorted(live._session_ids(str(req.get("platform") or "probe")))


def f_attested_relay_targets(req):
    try:
        return sorted(live.attested_relay_targets(req.get("platform")))
    except live.RelayRouteUnknown:
        return "unknown"


def f_is_unresolved_handle(req):
    token = req.get("native_token") if req.get("native_token_present") else live._UNSET
    return bool(live._is_unresolved_handle(req.get("platform"), str(req.get("target") or ""), token))


def f_authorize_relay_target(req):
    kwargs = {}
    if req.get("native_token_present"):
        kwargs["native_token"] = req.get("native_token")
    return live.authorize_relay_target(
        req.get("platform"), req.get("chat_id"), req.get("thread_id"), **kwargs
    )


DISPATCH = {
    "is_egress_decline": f_is_egress_decline,
    "declined_send": f_declined_send,
    "decline_error": f_decline_error,
    "decline_log_line": f_decline_log_line,
    "is_missing_gateway_relay": f_is_missing_gateway_relay,
    "has_live_native_adapter": f_has_live_native_adapter,
    "relay_routed_platform": f_relay_routed_platform,
    "session_ids": f_session_ids,
    "attested_relay_targets": f_attested_relay_targets,
    "is_unresolved_handle": f_is_unresolved_handle,
    "authorize_relay_target": f_authorize_relay_target,
}


def expected(req):
    func = req.get("func")
    handler = DISPATCH.get(func)
    if handler is None:
        raise AssertionError("unknown func %r" % (func,))
    install(req)
    return handler(req)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
