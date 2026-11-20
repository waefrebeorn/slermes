#!/usr/bin/env python3
"""Generate tests/fixtures/scale_to_zero_flow2.jsonl for the scale_to_zero_flow2
oracle (>=5 cases per ported func). Each case is {"func", "in": {..., "io": {...}}}
with io nested INSIDE in (the universal-seam script map). Script shapes: plain
value (reused), {"__seq__": [...]} (per-call, last repeats), {"__raise__": kind,
"msg": ...} (real exception). ASCII scope; integral floats == ints.

Domain (mirrors the port header): float() tokens printable ASCII without
quotes/backslashes, non-finite results excluded; path inputs str|null; object
platform elements carry scalar values only; same-type container comparisons
excluded; env/element str() positions exclude integral floats.

Run: python tests/gen_scale_to_zero_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "scale_to_zero_flow2.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def seq(*vals):
    return {"__seq__": list(vals)}


def raise_(kind, msg, **extra):
    d = {"__raise__": kind, "msg": msg}
    d.update(extra)
    return d


REL = "state/dashboard_clients.heartbeat"
HZ = "HERMES_SCALE_TO_ZERO"

# ── scale_to_zero_enabled ──
case("scale_to_zero_enabled", environ={HZ: "1"}, io={})
case("scale_to_zero_enabled", environ={HZ: "true"}, io={})
case("scale_to_zero_enabled", environ={HZ: "YES"}, io={})
case("scale_to_zero_enabled", environ={HZ: " On "}, io={})
case("scale_to_zero_enabled", environ={HZ: "0"}, io={})
case("scale_to_zero_enabled", environ={HZ: "false"}, io={})
case("scale_to_zero_enabled", environ={HZ: ""}, io={})
case("scale_to_zero_enabled", environ={HZ: "no"}, io={})
case("scale_to_zero_enabled", environ={HZ: "1\x1c"}, io={})
case("scale_to_zero_enabled", environ={HZ: "\x1c1\x1f"}, io={})
case("scale_to_zero_enabled", environ={HZ: True}, io={})
case("scale_to_zero_enabled", environ={HZ: 1}, io={})
case("scale_to_zero_enabled", environ={HZ: None}, io={})
case("scale_to_zero_enabled", environ={HZ: 0}, io={})
case("scale_to_zero_enabled", environ={}, io={})
case("scale_to_zero_enabled", environ=None, io={"os.environ": {HZ: "yes"}})
case("scale_to_zero_enabled", environ=None, io={"os.environ": None})
case("scale_to_zero_enabled", environ=None, io={})  # fail-closed os.environ
case("scale_to_zero_enabled", environ="x", io={})
case("scale_to_zero_enabled", environ=[], io={})
case("scale_to_zero_enabled", environ=None,
     io={"os.environ": raise_("OSError", "env boom")})
case("scale_to_zero_enabled", environ={HZ: "  TRUE  "}, io={})
case("scale_to_zero_enabled", environ={HZ: "2"}, io={})

# ── parse_idle_timeout_seconds ──
case("parse_idle_timeout_seconds", cfg_value=2, io={})
case("parse_idle_timeout_seconds", cfg_value=0, io={})
case("parse_idle_timeout_seconds", cfg_value=-1, io={})
case("parse_idle_timeout_seconds", cfg_value=0.5, io={})
case("parse_idle_timeout_seconds", cfg_value="2", io={})
case("parse_idle_timeout_seconds", cfg_value=" 2 ", io={})
case("parse_idle_timeout_seconds", cfg_value="1_0", io={})
case("parse_idle_timeout_seconds", cfg_value="1e1", io={})
case("parse_idle_timeout_seconds", cfg_value=".5", io={})
case("parse_idle_timeout_seconds", cfg_value="5.", io={})
case("parse_idle_timeout_seconds", cfg_value="+2", io={})
case("parse_idle_timeout_seconds", cfg_value="-2", io={})
case("parse_idle_timeout_seconds", cfg_value="abc", io={})
case("parse_idle_timeout_seconds", cfg_value="", io={})
case("parse_idle_timeout_seconds", cfg_value="1\x1c", io={})
case("parse_idle_timeout_seconds", cfg_value="\x1c1", io={})
case("parse_idle_timeout_seconds", cfg_value=" 1\t", io={})
case("parse_idle_timeout_seconds", cfg_value=None, io={})
case("parse_idle_timeout_seconds", cfg_value=True, io={})
case("parse_idle_timeout_seconds", cfg_value=False, io={})
case("parse_idle_timeout_seconds", cfg_value=[], io={})
case("parse_idle_timeout_seconds", cfg_value={}, io={})
case("parse_idle_timeout_seconds", cfg_value=2, default_minutes=None, io={})
case("parse_idle_timeout_seconds", cfg_value=0, default_minutes=None, io={})
case("parse_idle_timeout_seconds", cfg_value="abc", default_minutes="x", io={})
case("parse_idle_timeout_seconds", cfg_value=0, default_minutes=0, io={})
case("parse_idle_timeout_seconds", cfg_value=0, default_minutes=-5, io={})
case("parse_idle_timeout_seconds", cfg_value=0, default_minutes="3", io={})
case("parse_idle_timeout_seconds", cfg_value=0, default_minutes=True, io={})
case("parse_idle_timeout_seconds", cfg_value="1__0", io={})
case("parse_idle_timeout_seconds", cfg_value="1e", io={})
case("parse_idle_timeout_seconds", cfg_value="1.5e-2", io={})
case("parse_idle_timeout_seconds", cfg_value="0x10", io={})
case("parse_idle_timeout_seconds", cfg_value=2, default_minutes="bad", io={})
case("parse_idle_timeout_seconds", cfg_value="1_000.5_0e1_0", io={})
case("parse_idle_timeout_seconds", cfg_value="\t\n 3 \r", io={})
case("parse_idle_timeout_seconds", cfg_value="1_.0", io={})
case("parse_idle_timeout_seconds", cfg_value="1e5_0", io={})
case("parse_idle_timeout_seconds", cfg_value="-0.5", io={})

# ── messaging_is_relay_only_or_absent ──
case("messaging_is_relay_only_or_absent", platforms=[], io={})
case("messaging_is_relay_only_or_absent", platforms=["relay"], io={})
case("messaging_is_relay_only_or_absent", platforms=["RELAY"], io={})
case("messaging_is_relay_only_or_absent", platforms=[" relay "], io={})
case("messaging_is_relay_only_or_absent", platforms=["relay", "telegram"], io={})
case("messaging_is_relay_only_or_absent", platforms=["telegram"], io={})
case("messaging_is_relay_only_or_absent", platforms=["relay", "relay"], io={})
case("messaging_is_relay_only_or_absent", platforms=[{"value": "relay"}], io={})
case("messaging_is_relay_only_or_absent", platforms=[{"value": "Telegram"}], io={})
case("messaging_is_relay_only_or_absent", platforms=[{"value": "relay"}, {"value": "RELAY"}], io={})
case("messaging_is_relay_only_or_absent", platforms=["relay", {"value": "discord"}], io={})
case("messaging_is_relay_only_or_absent", platforms="relay", io={})
case("messaging_is_relay_only_or_absent", platforms="", io={})
case("messaging_is_relay_only_or_absent", platforms=None, io={})
case("messaging_is_relay_only_or_absent", platforms=5, io={})
case("messaging_is_relay_only_or_absent", platforms=True, io={})
case("messaging_is_relay_only_or_absent", platforms={"relay": 1}, io={})
case("messaging_is_relay_only_or_absent", platforms={"a": 1, "b": 2}, io={})
case("messaging_is_relay_only_or_absent", platforms=[5], io={})
case("messaging_is_relay_only_or_absent", platforms=[None], io={})
case("messaging_is_relay_only_or_absent", platforms=[True], io={})
case("messaging_is_relay_only_or_absent", platforms=[{"value": None}], io={})
case("messaging_is_relay_only_or_absent", platforms=[{}], io={})
case("messaging_is_relay_only_or_absent", platforms=[{"a": 1}], io={})
case("messaging_is_relay_only_or_absent", platforms=[{"value": "relay", "x": 1}], io={})
case("messaging_is_relay_only_or_absent", platforms=["telegram", "telegram"], io={})
case("messaging_is_relay_only_or_absent", platforms=["\x1crelay\x1f"], io={})
case("messaging_is_relay_only_or_absent", platforms=[{"value": True}], io={})
case("messaging_is_relay_only_or_absent", platforms=[{"value": 1}], io={})
case("messaging_is_relay_only_or_absent", platforms={"relay": 1, "RELAY": 2}, io={})
case("messaging_is_relay_only_or_absent", platforms=[" Relay ", "RELAY"], io={})
case("messaging_is_relay_only_or_absent", platforms=[""], io={})
case("messaging_is_relay_only_or_absent", platforms=2.5, io={})

# ── should_arm ──
case("should_arm", enabled=True, relay_only_or_absent=True, wake_url="https://x", io={})
case("should_arm", enabled=False, relay_only_or_absent=True, wake_url="https://x", io={})
case("should_arm", enabled=True, relay_only_or_absent=False, wake_url="https://x", io={})
case("should_arm", enabled=True, relay_only_or_absent=True, wake_url=None, io={})
case("should_arm", enabled=True, relay_only_or_absent=True, wake_url="", io={})
case("should_arm", enabled=True, relay_only_or_absent=True, wake_url=0, io={})
case("should_arm", enabled=1, relay_only_or_absent=1, wake_url="u", io={})
case("should_arm", enabled="yes", relay_only_or_absent=["relay"], wake_url="u", io={})
case("should_arm", enabled=None, relay_only_or_absent=None, wake_url=None, io={})
case("should_arm", enabled=0, relay_only_or_absent=1, wake_url="u", io={})
case("should_arm", enabled=True, relay_only_or_absent=True, wake_url=[], io={})
case("should_arm", enabled=True, relay_only_or_absent=True, wake_url={}, io={})

# ── is_idle ──
case("is_idle", active_work_count=0, seconds_since_last_inbound=120,
     idle_timeout_seconds=120, has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound=119,
     idle_timeout_seconds=120, has_live_background_work=False, io={})
case("is_idle", active_work_count=1, seconds_since_last_inbound=999,
     idle_timeout_seconds=120, has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound=999,
     idle_timeout_seconds=120, has_live_background_work=True, io={})
case("is_idle", active_work_count=-1, seconds_since_last_inbound=200,
     idle_timeout_seconds=120, has_live_background_work=False, io={})
case("is_idle", active_work_count=True, seconds_since_last_inbound=999,
     idle_timeout_seconds=120, has_live_background_work=False, io={})
case("is_idle", active_work_count=False, seconds_since_last_inbound=200,
     idle_timeout_seconds=120, has_live_background_work=False, io={})
case("is_idle", active_work_count="x", seconds_since_last_inbound=0,
     idle_timeout_seconds=0, has_live_background_work=False, io={})
case("is_idle", active_work_count=None, seconds_since_last_inbound=0,
     idle_timeout_seconds=0, has_live_background_work=False, io={})
case("is_idle", active_work_count=[], seconds_since_last_inbound=0,
     idle_timeout_seconds=0, has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound="a",
     idle_timeout_seconds="b", has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound="b",
     idle_timeout_seconds="a", has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound=5,
     idle_timeout_seconds="a", has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound=None,
     idle_timeout_seconds=5, has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound="a",
     idle_timeout_seconds=5, has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound=200,
     idle_timeout_seconds=120, has_live_background_work=1, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound=200,
     idle_timeout_seconds=120, has_live_background_work="", io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound=200,
     idle_timeout_seconds=120, has_live_background_work=[], io={})
case("is_idle", active_work_count=0.0, seconds_since_last_inbound=120.0,
     idle_timeout_seconds=120.0, has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound=[1],
     idle_timeout_seconds=5, has_live_background_work=False, io={})
case("is_idle", active_work_count=0, seconds_since_last_inbound=5,
     idle_timeout_seconds={}, has_live_background_work=False, io={})
case("is_idle", active_work_count=2, seconds_since_last_inbound="x",
     idle_timeout_seconds="y", has_live_background_work=True, io={})

# ── dashboard_client_heartbeat_path ──
case("dashboard_client_heartbeat_path", hermes_home="/tmp/h", io={})
case("dashboard_client_heartbeat_path", hermes_home="C:/Users/x", io={})
case("dashboard_client_heartbeat_path", hermes_home="C:\\Users\\x", io={})
case("dashboard_client_heartbeat_path", hermes_home="", io={})
case("dashboard_client_heartbeat_path", hermes_home=".", io={})
case("dashboard_client_heartbeat_path", hermes_home="./a//b/", io={})
case("dashboard_client_heartbeat_path", hermes_home="/", io={})
case("dashboard_client_heartbeat_path", hermes_home="//server/share", io={})
case("dashboard_client_heartbeat_path", hermes_home="///a", io={})
case("dashboard_client_heartbeat_path", hermes_home="a/../b", io={})
case("dashboard_client_heartbeat_path", hermes_home="a/./b", io={})
case("dashboard_client_heartbeat_path", hermes_home="C:", io={})
case("dashboard_client_heartbeat_path", hermes_home="C:foo", io={})
case("dashboard_client_heartbeat_path", hermes_home="C:.", io={})
case("dashboard_client_heartbeat_path", hermes_home="//", io={})
case("dashboard_client_heartbeat_path", hermes_home="//a/./b", io={})
case("dashboard_client_heartbeat_path", hermes_home="///a/./b", io={})
case("dashboard_client_heartbeat_path", hermes_home=None,
     io={"import_get_process_hermes_home": None, "get_process_hermes_home": "/h"})
case("dashboard_client_heartbeat_path", hermes_home=None,
     io={"import_get_process_hermes_home": raise_("ImportError", "no module")})
case("dashboard_client_heartbeat_path", hermes_home=None,
     io={"import_get_process_hermes_home": None,
         "get_process_hermes_home": raise_("OSError", "no home")})
case("dashboard_client_heartbeat_path", hermes_home=None,
     io={"import_get_process_hermes_home": None, "get_process_hermes_home": 5})
case("dashboard_client_heartbeat_path", hermes_home=None,
     io={"import_get_process_hermes_home": None, "get_process_hermes_home": None})
case("dashboard_client_heartbeat_path", hermes_home=None, io={})  # fail-closed import
case("dashboard_client_heartbeat_path", hermes_home=5, io={})
case("dashboard_client_heartbeat_path", hermes_home=None,
     io={"import_get_process_hermes_home": "ignored", "get_process_hermes_home": "/h"})
case("dashboard_client_heartbeat_path", hermes_home="a\\", io={})
case("dashboard_client_heartbeat_path", hermes_home="//a//b", io={})
case("dashboard_client_heartbeat_path", hermes_home="C://a", io={})
case("dashboard_client_heartbeat_path", hermes_home=" a ", io={})
case("dashboard_client_heartbeat_path", hermes_home=None,
     io={"import_get_process_hermes_home": raise_("SystemExit", "bye")})
case("dashboard_client_heartbeat_path", hermes_home="x:/", io={})
case("dashboard_client_heartbeat_path", hermes_home="1:", io={})

# ── touch_dashboard_client_heartbeat ──
def touch_io(**over):
    d = {"os.path.dirname": "/h/state", "os.makedirs": None,
         "open": {}, "close": None, "os.utime": None}
    d.update(over)
    return d


P = "/h/state/d.heartbeat"
case("touch_dashboard_client_heartbeat", path=P, io=touch_io())
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.path.dirname": raise_("OSError", "no dir"), "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.makedirs": raise_("PermissionError", "no"), "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"open": raise_("OSError", "no open"), "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"close": raise_("OSError", "no close"), "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.utime": raise_("OSError", "no utime"), "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.utime": raise_("OSError", "x"),
                    "logger.debug": raise_("RuntimeError", "log boom")}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.makedirs": raise_("SystemExit", "bye")}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.path.dirname": raise_("KeyboardInterrupt", "int")}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"open": raise_("GeneratorExit", "gen")}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.utime": raise_("CancelledError", "cancel")}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"open": None, "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"open": "x", "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"open": 5, "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=None,
     io=dict(touch_io(), **{"import_get_process_hermes_home": None,
                            "get_process_hermes_home": "/h"}))
case("touch_dashboard_client_heartbeat", path=None,
     io={"import_get_process_hermes_home": None, "get_process_hermes_home": 5,
         "logger.debug": None})
case("touch_dashboard_client_heartbeat", path=None,
     io={"import_get_process_hermes_home": raise_("SystemExit", "bye")})
case("touch_dashboard_client_heartbeat", path=None,
     io={"import_get_process_hermes_home": None,
         "get_process_hermes_home": raise_("ValueError", "bad home"),
         "logger.debug": None})
case("touch_dashboard_client_heartbeat", path=P, io={})  # fail-closed cascade
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.path.dirname": raise_("ValueError", "v"), "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.makedirs": raise_("BaseException", "base")}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"close": raise_("ValueError", "cv"), "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path="/x",
     io=touch_io(**{"os.path.dirname": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.utime": raise_("FileNotFoundError", "gone"), "logger.debug": None}))
case("touch_dashboard_client_heartbeat", path=P,
     io=touch_io(**{"os.path.dirname": raise_("OSError", "d"), "logger.debug": None}))

# ── dashboard_client_last_seen ──
LP = "/h/d.heartbeat"
case("dashboard_client_last_seen", path=LP, now=200, io={"os.stat": {"st_mtime": 100}})
case("dashboard_client_last_seen", path=LP, now=50, io={"os.stat": {"st_mtime": 100}})
case("dashboard_client_last_seen", path=LP, now=100, io={"os.stat": {"st_mtime": 100}})
case("dashboard_client_last_seen", path=LP, now=200,
     io={"os.stat": raise_("FileNotFoundError", "missing")})
case("dashboard_client_last_seen", path=LP, now=200,
     io={"os.stat": raise_("PermissionError", "no")})
case("dashboard_client_last_seen", path=LP, now=200,
     io={"os.stat": raise_("OSError", "os")})
case("dashboard_client_last_seen", path=LP, now=200,
     io={"os.stat": raise_("TimeoutError", "t")})
case("dashboard_client_last_seen", path=LP, now=200,
     io={"os.stat": raise_("ValueError", "v")})
case("dashboard_client_last_seen", path=LP, now=200,
     io={"os.stat": raise_("SystemExit", "bye")})
case("dashboard_client_last_seen", path=LP, now=200, io={"os.stat": None})
case("dashboard_client_last_seen", path=LP, now=200, io={"os.stat": {}})
case("dashboard_client_last_seen", path=LP, now=5, io={"os.stat": {"st_mtime": "x"}})
case("dashboard_client_last_seen", path=LP, now="b", io={"os.stat": {"st_mtime": "a"}})
case("dashboard_client_last_seen", path=LP, now="a", io={"os.stat": {"st_mtime": "b"}})
case("dashboard_client_last_seen", path=LP, now=None,
     io={"time.time": 1000, "os.stat": {"st_mtime": 500}})
case("dashboard_client_last_seen", path=LP, now=None,
     io={"time.time": raise_("OSError", "clock")})
case("dashboard_client_last_seen", path=None, now=20,
     io={"import_get_process_hermes_home": None, "get_process_hermes_home": "/h",
         "os.stat": {"st_mtime": 10}})
case("dashboard_client_last_seen", path=None, now=20,
     io={"import_get_process_hermes_home": None,
         "get_process_hermes_home": raise_("OSError", "home")})
case("dashboard_client_last_seen", path=None, now=20,
     io={"import_get_process_hermes_home": raise_("SystemExit", "bye")})
case("dashboard_client_last_seen", path=LP, now=200, io={})  # fail-closed os.stat
case("dashboard_client_last_seen", path=LP, now=0, io={"os.stat": {"st_mtime": -5}})
case("dashboard_client_last_seen", path=LP, now=0.5, io={"os.stat": {"st_mtime": True}})
case("dashboard_client_last_seen", path=LP, now=False, io={"os.stat": {"st_mtime": 0}})
case("dashboard_client_last_seen", path=LP, now=5, io={"os.stat": {"st_mtime": [1]}})
case("dashboard_client_last_seen", path=LP, now=[1],
     io={"os.stat": raise_("PermissionError", "no")})
case("dashboard_client_last_seen", path=LP, now=None,
     io={"time.time": None, "os.stat": {"st_mtime": 5}})
case("dashboard_client_last_seen", path=LP, now="x", io={"os.stat": {"st_mtime": 100}})
case("dashboard_client_last_seen", path=None, now=20,
     io={"import_get_process_hermes_home": None, "get_process_hermes_home": 5})
case("dashboard_client_last_seen", path=LP, now=None,
     io={"time.time": 7, "os.stat": raise_("FileNotFoundError", "missing")})
case("dashboard_client_last_seen", path=LP, now=2.5, io={"os.stat": {"st_mtime": 2.5}})
case("dashboard_client_last_seen", path=LP, now=200,
     io={"os.stat": {"st_mtime": 100, "extra": 1}})
case("dashboard_client_last_seen", path=LP, now=200,
     io={"os.stat": raise_("KeyboardInterrupt", "int")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-40s %d" % (k, counts[k]))
    assert all(v >= 5 for v in counts.values()), "need >=5 cases per func"
    assert len(counts) == 8, "expected 8 funcs"


if __name__ == "__main__":
    main()