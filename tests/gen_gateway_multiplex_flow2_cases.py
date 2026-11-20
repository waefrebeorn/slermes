#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_multiplex_flow2.jsonl — >=5 cases per
ported func for the gateway_multiplex_flow2 differential oracle (10 funcs).

Seam scripting convention (matches harness script_call / oracle seam):
- a Python LIST script is a QUEUE: one result per call (last repeats);
- a scalar / dict / None script repeats verbatim every call;
- {"__raise__": cls, "msg": ...} scripts an exception (Exception family folded
  by notify's `except Exception`; the BaseException trio passes through).

Fixture domain (kept identical on both sides, see the port header):
- paths are POSIX strings or null (null home -> Path(None) TypeError);
- root scripts (get_default_hermes_root) are str|null only;
- str(p) elements are scalars (str/int/bool/null), no floats;
- served_profiles iterates list -> elements / str -> chars / dict -> keys /
  falsy -> [] / truthy scalar -> TypeError not iterable.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_multiplex_flow2.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def rt(**kw):
    return kw


# ── 1. live_default_gateway_pid ──
case("live_default_gateway_pid", seams={"get_default_hermes_root": "/h",
                                        "live_gateway_pid_for_home": 1234})
case("live_default_gateway_pid", seams={"get_default_hermes_root": None,
                                        "live_gateway_pid_for_home": None})
case("live_default_gateway_pid", seams={"get_default_hermes_root": "//srv/profiles/p1/",
                                        "live_gateway_pid_for_home": 7})
case("live_default_gateway_pid", seams={"get_default_hermes_root": R("RuntimeError", "boom")})
case("live_default_gateway_pid", seams={"get_default_hermes_root": R("SystemExit", "1")})
case("live_default_gateway_pid", seams={"get_default_hermes_root": "/h",
                                        "live_gateway_pid_for_home": R("FileNotFoundError", "gone")})

# ── 2. recorded_served_profiles ──
case("recorded_served_profiles", default_root="/h",
     seams={"live_gateway_pid_for_home": None})
case("recorded_served_profiles", default_root="/h",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(served_profiles=["p1", "default", None, 0, True])})
case("recorded_served_profiles", default_root=None,
     seams={"get_default_hermes_root": "/h/x/", "live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(served_profiles="notalist")})
case("recorded_served_profiles", default_root="/h",
     seams={"live_gateway_pid_for_home": 1, "read_runtime_status": None})
case("recorded_served_profiles", default_root="/h",
     seams={"live_gateway_pid_for_home": 1, "read_runtime_status": 5})
case("recorded_served_profiles", default_root="/h",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": R("OSError", "io")})
case("recorded_served_profiles", default_root="",
     seams={"get_default_hermes_root": "/d", "live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(served_profiles=[])})
case("recorded_served_profiles", default_root="/h",
     seams={"live_gateway_pid_for_home": R("ValueError", "pid")})
case("recorded_served_profiles", default_root="/h",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(served_profiles={"p1": 1})})

# ── 3. multiplexer_served_secondaries ──
case("multiplexer_served_secondaries",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(served_profiles=["p1", "default", "p2", "", None])})
case("multiplexer_served_secondaries", seams={"live_gateway_pid_for_home": None})
case("multiplexer_served_secondaries",
     seams={"live_gateway_pid_for_home": 1, "read_runtime_status": rt(served_profiles=[])})
case("multiplexer_served_secondaries",
     seams={"live_gateway_pid_for_home": 1, "read_runtime_status": rt(served_profiles="ab")})
case("multiplexer_served_secondaries",
     seams={"live_gateway_pid_for_home": 1, "read_runtime_status": rt(served_profiles=["default"])})
case("multiplexer_served_secondaries",
     seams={"live_gateway_pid_for_home": R("RuntimeError", "x")})

# ── 4. served_profile_unserved_platforms ──
case("served_profile_unserved_platforms", profile="", seams={})
case("served_profile_unserved_platforms", profile=None, seams={})
case("served_profile_unserved_platforms", profile="p1",
     seams={"live_gateway_pid_for_home": None})
case("served_profile_unserved_platforms", profile="p1",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(platforms={
                "p1:telegram": {"error_code": "whatsapp_unpaired", "error_message": "pair me"},
                "p1:wa": {"error_code": "multiplex_shared_ingress"},
                "p1:ok": {"error_code": "other"},
                "p2:telegram": {"error_code": "whatsapp_unpaired"}})})
case("served_profile_unserved_platforms", profile="p1",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(platforms={
                "p1:a": {"error_code": "whatsapp_unpaired", "error_message": ""},
                "p1:b": {"error_code": "whatsapp_unpaired", "error_message": 0},
                "p1:c": {"error_code": "whatsapp_unpaired", "error_message": ["x"]}})})
case("served_profile_unserved_platforms", profile="p1",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(platforms={
                "p1:a": {"error_code": ["unhashable"]}})})
case("served_profile_unserved_platforms", profile="p1",
     seams={"live_gateway_pid_for_home": 1, "read_runtime_status": 7})
case("served_profile_unserved_platforms", profile="p1",
     seams={"live_gateway_pid_for_home": 1, "read_runtime_status": rt(platforms="nope")})
case("served_profile_unserved_platforms", profile="p1",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": R("OSError", "io")})

# ── 5. served_profile_ingress_urls ──
case("served_profile_ingress_urls", seams={"live_gateway_pid_for_home": None})
case("served_profile_ingress_urls",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(platforms={
                "p1:telegram": {"ingress_url": "https://t"},
                "p1:wa": {"ingress_url": "https://w", "state": "fatal"},
                "default:api": {"ingress_url": "https://a"},
                "bad": {"ingress_url": "https://b"},
                "p1:no": {"state": "stopped"}})})
case("served_profile_ingress_urls", profile="p1",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(platforms={
                "p1:telegram": {"ingress_url": "https://t"},
                "p2:telegram": {"ingress_url": "https://x"}})})
case("served_profile_ingress_urls",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(served_profiles=["p2"],
                                      platforms={}),
            "shared_listener_mirror_platforms": {"api": {"ingress_url": "https://api"},
                                                 "none": {}}})
case("served_profile_ingress_urls",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(served_profiles=["p2"]),
            "shared_listener_mirror_platforms": "notadict"})
case("served_profile_ingress_urls",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(served_profiles=5)})
case("served_profile_ingress_urls",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": R("OSError", "io")})
case("served_profile_ingress_urls", profile="p1",
     seams={"live_gateway_pid_for_home": 1,
            "read_runtime_status": rt(served_profiles=["p1"],
                                      platforms={"p1:x": {"ingress_url": 42}}),
            "shared_listener_mirror_platforms": R("RuntimeError", "mirror")})

# ── 6. format_ingress_url_lines ──
case("format_ingress_url_lines", urls={"telegram": "https://t", "api": "https://a"})
case("format_ingress_url_lines", urls={"b": "2", "a": "1"}, indent="")
case("format_ingress_url_lines", urls={"z": "z", "y": "y"}, indent="    ")
case("format_ingress_url_lines", urls={})
case("format_ingress_url_lines", urls=["not", "a", "dict"])
case("format_ingress_url_lines", urls={"n": 5, "t": True, "z": None})
case("format_ingress_url_lines", urls={"a": "b"}, indent=None)

# ── 7. notify_multiplexer_profiles_changed ──
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": None})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": 1,
            "rescan_gateway_profiles": {"multiplex": True, "served_profiles": ["p1", "p2"]}})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": 1,
            "rescan_gateway_profiles": {"multiplex": False, "served_profiles": ["p1"]}})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": 1, "rescan_gateway_profiles": "nope"})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": 1,
            "rescan_gateway_profiles": {"multiplex": True, "served_profiles": "no"}})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": 1,
            "rescan_gateway_profiles": R("RuntimeError", "fail")})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"get_default_hermes_root": R("ValueError", "bad")})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": R("FileNotFoundError", "gone")})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": R("SystemExit", "1")})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": R("KeyboardInterrupt", "")})
case("notify_multiplexer_profiles_changed", profile_name="p1", timeout=3.5,
     seams={"live_gateway_pid_for_home": 1,
            "rescan_gateway_profiles": {"multiplex": True, "served_profiles": []}})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": R("RuntimeError", "x"),
            "logger_debug": R("RuntimeError", "handler")})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": R("ExceptionGroup", "groups")})
case("notify_multiplexer_profiles_changed", profile_name="p1",
     seams={"live_gateway_pid_for_home": R("BaseExceptionGroup", "groups")})

# ── 8. multiplexer_root_for ──
case("multiplexer_root_for", home="/h/profiles/p1",
     seams={"named_profile_home": "p1"})
case("multiplexer_root_for", home="/h/profiles/p1",
     seams={"named_profile_home": None})
case("multiplexer_root_for", home=None, seams={})
case("multiplexer_root_for", home="/h", seams={"named_profile_home": True})
case("multiplexer_root_for", home="//srv/profiles/p1",
     seams={"named_profile_home": 1})
case("multiplexer_root_for", home="/h/profiles/p1",
     seams={"named_profile_home": R("RuntimeError", "x")})
case("multiplexer_root_for", home=".", seams={"named_profile_home": "x"})
case("multiplexer_root_for", home="/h/profiles/p1/./",
     seams={"named_profile_home": "p1"})

# ── 9. implied_host_root ──
case("implied_host_root", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": False})
case("implied_host_root", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": True})
case("implied_host_root", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": None})
case("implied_host_root", home="/h/profiles/p1",
     seams={"named_profile_home": None})
case("implied_host_root", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": 0})
case("implied_host_root", home=None, seams={})
case("implied_host_root", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": R("OSError", "x")})

# ── 10. multiplexer_serves_home ──
case("multiplexer_serves_home", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": True})
case("multiplexer_serves_home", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": False})
case("multiplexer_serves_home", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": None,
            "read_runtime_status": rt(served_profiles=["p1", "p2"]),
            "normalize_profile_name": ["p1", "p2", "p1"]})
case("multiplexer_serves_home", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": None,
            "read_runtime_status": rt(served_profiles=["other"]),
            "normalize_profile_name": ["other", "p1"]})
case("multiplexer_serves_home", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": None,
            "read_runtime_status": rt(served_profiles="notalist")})
case("multiplexer_serves_home", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": None,
            "read_runtime_status": rt(served_profiles=["p1"]),
            "normalize_profile_name": R("RuntimeError", "norm")})
case("multiplexer_serves_home", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": None,
            "read_runtime_status": rt(served_profiles=["p1"]),
            "normalize_profile_name": ["p1", {"unhashable": 1}]})
case("multiplexer_serves_home", home="/h/profiles/p1",
     seams={"named_profile_home": "p1", "host_serves_named_profile": 0})
case("multiplexer_serves_home", home=None, seams={})


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
        print("  %-40s %d" % (fn, counts[fn]))
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit("funcs with <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
