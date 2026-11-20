#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_enroll_flow.jsonl — >=5 cases per ported
func for the gateway_enroll_flow differential oracle (21 funcs).

Seam scripting convention (matches the harness script_call / oracle seam):
- a Python LIST script is a QUEUE: one result per call (last repeats);
- a LIST-returning seam therefore wraps each list result one level deeper:
  L([a, b]) == [[a, b]] (a queue whose first result is the list [a, b]);
- a scalar / dict / None script repeats verbatim every call;
- {"__raise__": cls, "msg": ...} scripts an exception.

Fixture inputs are ASCII; the non-ASCII warning sign / box-drawing chars
appear only in the fixed live constants the port emits. Paths are non-empty
absolute POSIX strings; pids are int literals or null.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_enroll_flow.jsonl")
CASES = []

SINGLE = "only one profile exists (nothing to multiplex)"
STANDALONE = "this profile is standalone (gateway.standalone: true); it serves only itself"


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def L(x):
    """Wrap a list result as a single-element queue (repeats every call)."""
    return [x]


def R(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def dec(enabled, source, reason=""):
    return {"enabled": enabled, "source": source, "reason": reason}


def plan(secondaries=None, blocked=False, blockers=None):
    return {"standalone_secondaries": secondaries or [], "blocked": blocked,
            "blockers": blockers or []}


def launcher(name="default", standalone=False, home="/h"):
    """Seams for _standalone_launcher: name None/'default' short-circuits."""
    d = {"get_hermes_home": home, "profile_name_for_home": name}
    if name not in (None, "default"):
        d["profile_is_standalone"] = standalone
    return d


def no_blocker(name="default"):
    """Seams settling implicit_multiplex_blocker to None (2 profiles, clean)."""
    d = launcher(name)
    d.update({"profiles_to_serve": L([["default", "/h"], ["p1", "/h/p1"]]),
              "host_supports_migration": None,
              "build_migration_plan": plan()})
    return d


# ── 1. fail ──
case("fail", lines=["a"])
case("fail", lines=["a", "b"])
case("fail", lines=[])
case("fail", lines=[5, True])
case("fail", lines=["x"], seams={"sys_exit": R("RuntimeError", "boom")})
case("fail", lines=["x"], seams={"print": R("SystemExit", "9")})
case("fail", lines=["x"], seams={})

# ── 2. explicit_multiplex_flag ──
case("explicit_multiplex_flag", default_home="/h", seams={"env_multiplex_override": True})
case("explicit_multiplex_flag", default_home="/h", seams={"env_multiplex_override": False})
case("explicit_multiplex_flag", default_home="/h",
     seams={"env_multiplex_override": None, "config_exists": False})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {"multiplex_profiles": True}})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {"multiplex_profiles": False}})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {"multiplex_profiles": "yes"},
            "bool_token": True})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {"multiplex_profiles": "maybe"},
            "bool_token": None})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True,
            "read_user_config_raw": {"multiplex_profiles": None,
                                     "gateway": {"multiplex_profiles": True}}})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True,
            "read_user_config_raw": {"gateway": {"multiplex_profiles": "no"}},
            "bool_token": False})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True,
            "read_user_config_raw": {"gateway": "notdict", "multiplex_profiles": 1}})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": None})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": R("ValueError", "bad yaml")})
case("explicit_multiplex_flag", default_home="/h", seams={"config_exists": R("SystemExit", "1")})
case("explicit_multiplex_flag", default_home=None, seams={"env_multiplex_override": True})
case("explicit_multiplex_flag", default_home=None, seams={"env_multiplex_override": None})
case("explicit_multiplex_flag", default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {"multiplex_profiles": 0}})

# ── 3. settled_explicit_flag ──
for v in (True, False, None, 1, 0, "x", ""):
    case("settled_explicit_flag", value=v)

# ── 4. host_serves_named_profile ──
case("host_serves_named_profile", default_home="/h", home="/h/p1",
     seams={"profile_is_standalone": True})
case("host_serves_named_profile", default_home="/h", home="/h/p1",
     seams={"profile_is_standalone": False, "env_multiplex_override": True})
case("host_serves_named_profile", default_home="/h", home="/h/p1",
     seams={"profile_is_standalone": False, "env_multiplex_override": False})
case("host_serves_named_profile", default_home="/h", home="/h/p1",
     seams={"profile_is_standalone": False, "env_multiplex_override": None,
            "config_exists": False})
case("host_serves_named_profile", default_home="/h", home="/h/p1",
     seams={"profile_is_standalone": R("RuntimeError", "x")})
case("host_serves_named_profile", default_home="/h", home="/h/p1",
     seams={"profile_is_standalone": False, "env_multiplex_override": R("SystemExit", "1")})
case("host_serves_named_profile", default_home="/h", home="/h/p1",
     seams={"profile_is_standalone": "yes"})

# ── 5. default_gateway_multiplexes ──
case("default_gateway_multiplexes", default_home="/h",
     seams={"recorded_served_profiles": L(["p1"])})
case("default_gateway_multiplexes", default_home="/h",
     seams={"recorded_served_profiles": L([])})
case("default_gateway_multiplexes", default_home="/h",
     seams={"recorded_served_profiles": None, "env_multiplex_override": True})
case("default_gateway_multiplexes", default_home="/h",
     seams={"recorded_served_profiles": None, "env_multiplex_override": False})
case("default_gateway_multiplexes", default_home="/h",
     seams={"recorded_served_profiles": None, "env_multiplex_override": None,
            "config_exists": False})
case("default_gateway_multiplexes", default_home=None,
     seams={"get_default_hermes_root": "/h", "recorded_served_profiles": L(["a", "b"])})
case("default_gateway_multiplexes", default_home="/h",
     seams={"recorded_served_profiles": R("OSError", "x")})
case("default_gateway_multiplexes", default_home="/h",
     seams={"recorded_served_profiles": None, "config_exists": True,
            "read_user_config_raw": {"multiplex_profiles": True}})

# ── 6. standalone_launcher ──
case("standalone_launcher", seams=launcher(None))
case("standalone_launcher", seams=launcher("default"))
case("standalone_launcher", seams=launcher("p1", True))
case("standalone_launcher", seams=launcher("p1", False))
case("standalone_launcher", seams=launcher("", True))
case("standalone_launcher", seams={"get_hermes_home": R("RuntimeError", "x")})
case("standalone_launcher", seams={"get_hermes_home": "/h",
                                   "profile_name_for_home": R("SystemExit", "1")})
case("standalone_launcher", seams={"get_hermes_home": "/h", "profile_name_for_home": "p1",
                                   "profile_is_standalone": R("ValueError", "x")})

# ── 7. standalone_launcher_decision ──
case("standalone_launcher_decision", config={}, seams=launcher("default"))
case("standalone_launcher_decision", config={}, seams=launcher("p1", True))
case("standalone_launcher_decision", config={"multiplex_profiles": True, "other": 1},
     seams=launcher("p1", True))
case("standalone_launcher_decision", config=None, seams=launcher("p1", True))
case("standalone_launcher_decision", config=None, seams=launcher("default"))
case("standalone_launcher_decision", config={"multiplex_profiles": True},
     seams={"get_hermes_home": "/h", "profile_name_for_home": "p1",
            "profile_is_standalone": R("KeyboardInterrupt", "")})

# ── 8. implicit_multiplex_blocker ──
case("implicit_multiplex_blocker", seams=launcher("p1", True))
case("implicit_multiplex_blocker",
     seams=dict(launcher("default"), profiles_to_serve=L([["default", "/h"]])))
case("implicit_multiplex_blocker",
     seams=dict(launcher("default"), profiles_to_serve=L([["default", "/h"], ["p1", "/h/p1"]]),
                host_supports_migration="s6 host"))
case("implicit_multiplex_blocker",
     seams=dict(no_blocker(), build_migration_plan=plan([{"name": "p1", "pid": 123}])))
case("implicit_multiplex_blocker",
     seams=dict(no_blocker(),
                build_migration_plan=plan([{"name": "p1", "pid": None}, {"name": "p2", "pid": 0}]),
                pg_service_label=["svc-p1", "svc-p2"]))
case("implicit_multiplex_blocker",
     seams=dict(no_blocker(), build_migration_plan=plan([], True, ["dup bot", "port fight"])))
case("implicit_multiplex_blocker", seams=no_blocker())
case("implicit_multiplex_blocker",
     seams=dict(launcher("default"), profiles_to_serve=None))
case("implicit_multiplex_blocker",
     seams=dict(no_blocker(), build_migration_plan=None))
case("implicit_multiplex_blocker",
     seams=dict(no_blocker(), build_migration_plan={}))
case("implicit_multiplex_blocker",
     seams=dict(no_blocker(), build_migration_plan=plan([{"name": "p1"}])))
case("implicit_multiplex_blocker",
     seams=dict(no_blocker(), build_migration_plan=plan([{"name": "p1", "pid": None}]),
                pg_service_label=R("RuntimeError", "x")))

# ── 9. default_profile_home ──
case("default_profile_home", seams={"get_default_hermes_root": "/h"})
case("default_profile_home", seams={"get_default_hermes_root": None})
case("default_profile_home", seams={"get_default_hermes_root": R("RuntimeError", "x")})
case("default_profile_home", seams={"get_default_hermes_root": R("SystemExit", "1")})
case("default_profile_home", seams={"get_default_hermes_root": "/h/x/"})
case("default_profile_home", seams={})

# ── 10. persist_resolved_default ──
case("persist_resolved_default", decision=dec(False, "config"), default_home="/h", seams={})
case("persist_resolved_default", decision=dec(True, "guard"), default_home="/h", seams={})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {"multiplex_profiles": True}})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {}})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {"multiplex_profiles": False}})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {"multiplex_profiles": "x"}})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": False})
case("persist_resolved_default", decision=dec(True, "retired-opt-out"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {}})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": R("ValueError", "x")})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {},
            "write_multiplex_flag": R("OSError", "x")})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {},
            "write_multiplex_flag": R("SystemExit", "1")})
case("persist_resolved_default", decision=dec(True, "retired-opt-out"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {},
            "marker_write_text": R("OSError", "x")})
case("persist_resolved_default", decision=dec(True, "config"), default_home=None,
     seams={"get_default_hermes_root": "/h", "config_exists": False})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True,
            "read_user_config_raw": {"multiplex_profiles": None,
                                     "gateway": {"multiplex_profiles": True}}})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True,
            "read_user_config_raw": {"gateway": {"multiplex_profiles": True}}})
case("persist_resolved_default", decision=dec(True, "config"), default_home="/h",
     seams={"config_exists": True, "read_user_config_raw": {},
            "logger_info": R("RuntimeError", "x")})

# ── 11. retired_opt_out_notice_lines (pure constant) ──
for _ in range(5):
    case("retired_opt_out_notice_lines")

# ── 12. consume_rewritten_notice ──
case("consume_rewritten_notice", default_home="/h", seams={"marker_exists": False})
case("consume_rewritten_notice", default_home="/h",
     seams={"marker_exists": True, "marker_unlink": None})
case("consume_rewritten_notice", default_home="/h",
     seams={"marker_exists": True, "marker_unlink": R("OSError", "x")})
case("consume_rewritten_notice", default_home="/h",
     seams={"marker_exists": True, "marker_unlink": R("FileNotFoundError", "x")})
case("consume_rewritten_notice", default_home="/h",
     seams={"marker_exists": True, "marker_unlink": R("RuntimeError", "x")})
case("consume_rewritten_notice", default_home="/h",
     seams={"marker_exists": True, "marker_unlink": R("SystemExit", "1")})
case("consume_rewritten_notice", default_home=None,
     seams={"get_default_hermes_root": "/h", "marker_exists": False})
case("consume_rewritten_notice", default_home="/h", seams={"marker_exists": R("ValueError", "x")})

# ── 13. box ──
case("box", body=["ab", "c"])
case("box", body=["hello"])
case("box", body=[])
case("box", body=None)
case("box", body=["long line here", "x"])
case("box", body=[5])
case("box", body=[["a"]])
case("box", body=["", ""])

# ── 14. resolve_multiplex_mode ──
case("resolve_multiplex_mode", config={}, seams=launcher("p1", True))
case("resolve_multiplex_mode", config={"multiplex_profiles": True}, seams=launcher("default"))
case("resolve_multiplex_mode", config={"multiplex_profiles": False}, seams=no_blocker())
case("resolve_multiplex_mode", config={}, seams=no_blocker())
case("resolve_multiplex_mode", config={},
     seams=dict(no_blocker(), host_supports_migration="s6 host"))
case("resolve_multiplex_mode", config={},
     seams=dict(launcher("default"), profiles_to_serve=None))
case("resolve_multiplex_mode", config={},
     seams={"get_hermes_home": "/h", "profile_name_for_home": "p1",
            "profile_is_standalone": R("SystemExit", "1")})
case("resolve_multiplex_mode", config={},
     seams=dict(launcher("default"), profiles_to_serve=L([["default", "/h"], ["p1", "/h/p1"]]),
                host_supports_migration=None,
                build_migration_plan=R("KeyboardInterrupt", "")))
case("resolve_multiplex_mode", config=None, seams=no_blocker())
case("resolve_multiplex_mode", config=None, seams=launcher("p1", True))
case("resolve_multiplex_mode", config={"multiplex_profiles": False},
     seams=dict(no_blocker(), host_supports_migration="blocker"))
case("resolve_multiplex_mode", config={"multiplex_profiles": 1}, seams=launcher("default"))

# ── 15. record_multiplex_decision ──
case("record_multiplex_decision", decision=dec(False, "guard", "why"), pending=[],
     seams={"owns_gateway_runtime_lock": False})
case("record_multiplex_decision", decision=dec(False, "guard", "why"),
     pending=[dec(True, "default")], seams={"owns_gateway_runtime_lock": False})
case("record_multiplex_decision", decision=dec(False, "guard", "why"), pending=[],
     seams={"owns_gateway_runtime_lock": True})
case("record_multiplex_decision", decision=dec(True, "default"), pending=[],
     seams={"owns_gateway_runtime_lock": True})
case("record_multiplex_decision", decision=dec(False, "guard", ""), pending=[],
     seams={"owns_gateway_runtime_lock": True})
case("record_multiplex_decision", decision=dec(False, "guard", "why"), pending=[],
     seams={"owns_gateway_runtime_lock": R("RuntimeError", "x")})
case("record_multiplex_decision", decision=dec(False, "guard", "why"), pending=[],
     seams={"owns_gateway_runtime_lock": True, "write_runtime_status": R("OSError", "x")})
case("record_multiplex_decision", decision=dec(False, "guard", "why"), pending=[],
     seams={"owns_gateway_runtime_lock": True, "write_runtime_status": R("SystemExit", "1")})
case("record_multiplex_decision", decision=dec(False, "guard", None), pending=[],
     seams={"owns_gateway_runtime_lock": True})

# ── 16. publish_pending_multiplex_decision ──
case("publish_pending_multiplex_decision", pending=[], seams={})
case("publish_pending_multiplex_decision", pending=[dec(False, "guard", "why")],
     seams={"owns_gateway_runtime_lock": False})
case("publish_pending_multiplex_decision",
     pending=[dec(True, "default"), dec(False, "guard", "why2")],
     seams={"owns_gateway_runtime_lock": True})
case("publish_pending_multiplex_decision", pending=[dec(False, "guard", "why")],
     seams={"owns_gateway_runtime_lock": R("ValueError", "x")})
case("publish_pending_multiplex_decision", pending=[dec(False, "guard", "why")],
     seams={"owns_gateway_runtime_lock": True, "write_runtime_status": R("SystemExit", "1")})

# ── 17. log_multiplex_decision ──
case("log_multiplex_decision", decision=dec(True, "retired-opt-out"), pending=[],
     seams={"owns_gateway_runtime_lock": True, "config_exists": False})
case("log_multiplex_decision", decision=dec(False, "guard", SINGLE), pending=[],
     seams={"owns_gateway_runtime_lock": True})
case("log_multiplex_decision", decision=dec(False, "guard", "other"), pending=[],
     seams=dict({"owns_gateway_runtime_lock": True},
                **launcher("default"),
                profiles_to_serve=L([["default", "/h"], ["p1", "/h/p1"]])))
case("log_multiplex_decision", decision=dec(False, "guard", "other"), pending=[],
     seams=dict({"owns_gateway_runtime_lock": True},
                **launcher("default"), profiles_to_serve=L([["default", "/h"]])))
case("log_multiplex_decision", decision=dec(True, "default"), pending=[],
     seams={"owns_gateway_runtime_lock": True, "config_exists": False})
case("log_multiplex_decision", decision=dec(True, "config"), pending=[],
     seams={"owns_gateway_runtime_lock": True})
case("log_multiplex_decision", decision=dec(False, "guard", "other"), pending=[],
     seams=dict({"owns_gateway_runtime_lock": True},
                **launcher("default"), profiles_to_serve=R("RuntimeError", "x")))
case("log_multiplex_decision", decision=dec(False, "guard", "other"), pending=[],
     seams={"owns_gateway_runtime_lock": True, "write_runtime_status": R("SystemExit", "1")})

# ── 18. unserved_profiles ──
case("unserved_profiles",
     seams=dict(launcher("default"), profiles_to_serve=L([["default", "/h"], ["p1", "/h/p1"]])))
case("unserved_profiles",
     seams=dict(launcher("p1"), profiles_to_serve=L([["default", "/h"], ["p1", "/h/p1"]])))
case("unserved_profiles",
     seams=dict(launcher(None), profiles_to_serve=L([["default", "/h"], ["p1", "/h/p1"]])))
case("unserved_profiles",
     seams=dict(launcher(""), profiles_to_serve=L([["default", "/h"], ["p1", "/h/p1"]])))
case("unserved_profiles", seams=dict(launcher("default"), profiles_to_serve=None))
case("unserved_profiles",
     seams=dict(launcher("default"), profiles_to_serve=L([["default", "/h", "extra"]])))
case("unserved_profiles", seams=dict(launcher("default"), profiles_to_serve=L([["default"]])))
case("unserved_profiles", seams=dict(launcher("default"), profiles_to_serve=L([5])))
case("unserved_profiles", seams=dict(launcher("default"), profiles_to_serve=R("OSError", "x")))

# ── 19. standalone_warning_lines ──
case("standalone_warning_lines", decision=dec(True, "config"), seams={})
case("standalone_warning_lines", decision=dec(False, "guard", SINGLE), seams={})
case("standalone_warning_lines", decision=dec(False, "guard", "why"),
     unserved=["p1", "p2"], seams={})
case("standalone_warning_lines", decision=dec(False, "guard", "why"), unserved=None,
     seams=dict(launcher("default"), profiles_to_serve=L([["default", "/h"], ["p1", "/h/p1"]])))
case("standalone_warning_lines", decision=dec(False, "guard", "why"), unserved=None,
     seams=dict(launcher("default"), profiles_to_serve=R("RuntimeError", "x")))
case("standalone_warning_lines", decision=dec(False, "guard", "why"), unserved=[], seams={})
case("standalone_warning_lines", decision=dec(False, "guard", "why"), unserved=None,
     seams=dict(launcher("default"), profiles_to_serve=L([["default", "/h"]])))
case("standalone_warning_lines", decision=dec(False, "guard", "why"),
     unserved=["p1", 5], seams={})
case("standalone_warning_lines", decision=dec(False, 5, "why"), seams={})

# ── 20. recorded_standalone_reason ──
case("recorded_standalone_reason", seams={"read_runtime_status": None})
case("recorded_standalone_reason", seams={"read_runtime_status": {"gateway_state": "stopped"}})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": "startup_failed"}})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": "running",
                                    "multiplex_standalone_reason": "why"},
            "runtime_status_pid_is_live": True})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": "running",
                                    "multiplex_standalone_reason": "why"},
            "runtime_status_pid_is_live": False})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": "running",
                                    "multiplex_standalone_reason": 5},
            "runtime_status_pid_is_live": True})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": "running",
                                    "multiplex_standalone_reason": ""},
            "runtime_status_pid_is_live": True})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": "running"},
            "runtime_status_pid_is_live": True})
case("recorded_standalone_reason", seams={"read_runtime_status": R("OSError", "x")})
case("recorded_standalone_reason", seams={"read_runtime_status": R("RuntimeError", "x")})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": "running"},
            "runtime_status_pid_is_live": R("ValueError", "x")})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": "running"},
            "runtime_status_pid_is_live": R("SystemExit", "1")})
case("recorded_standalone_reason", seams={"read_runtime_status": "garbled"})
case("recorded_standalone_reason", state_path="/h/gateway_state.json",
     seams={"read_runtime_status": {"gateway_state": "running",
                                    "multiplex_standalone_reason": "why"},
            "runtime_status_pid_is_live": True})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": None,
                                    "multiplex_standalone_reason": "why"}})
case("recorded_standalone_reason",
     seams={"read_runtime_status": {"gateway_state": "running"},
            "runtime_status_pid_is_live": R("JSONDecodeError", "x")})  # non-builtin -> custom Exception, propagates

# ── 21. recorded_standalone_warning_lines ──
case("recorded_standalone_warning_lines",
     seams=dict({"read_runtime_status": {"gateway_state": "running",
                                         "multiplex_standalone_reason": "why"},
                 "runtime_status_pid_is_live": True},
                **launcher("default"),
                profiles_to_serve=L([["default", "/h"], ["p1", "/h/p1"]])))
case("recorded_standalone_warning_lines", seams={"read_runtime_status": None})
case("recorded_standalone_warning_lines",
     seams={"read_runtime_status": {"gateway_state": "running",
                                    "multiplex_standalone_reason": "why"},
            "runtime_status_pid_is_live": False})
case("recorded_standalone_warning_lines",
     seams=dict({"read_runtime_status": {"gateway_state": "running",
                                         "multiplex_standalone_reason": "why"},
                 "runtime_status_pid_is_live": True},
                **launcher("default"), profiles_to_serve=L([["default", "/h"]])))
case("recorded_standalone_warning_lines", seams={"read_runtime_status": R("RuntimeError", "x")})
case("recorded_standalone_warning_lines",
     seams={"read_runtime_status": {"gateway_state": "running"},
            "runtime_status_pid_is_live": R("SystemExit", "1")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        print("  %-38s %d" % (fn, counts[fn]))
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit("funcs with <5 cases: %s" % thin)


if __name__ == "__main__":
    main()