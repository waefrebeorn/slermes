#!/usr/bin/env python3
"""gen_rest_revive1_cases.py — emit tests/fixtures/rest_revive1.jsonl and
validate every case through the oracle's PYTHON side standalone (extract +
drive), asserting hand-checked expectations. No harness compile/link here.

Usage: python tests/gen_rest_revive1_cases.py [--validate-only]
"""
import importlib.util
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FIXTURE = REPO / "tests" / "fixtures" / "rest_revive1.jsonl"

CASES = []


def add(func, inp, expect=None):
    CASES.append({"func": func, "in": inp, "_expect": expect})


def script(ret=None, raise_=None):
    d = {"__script__": True}
    if raise_ is not None:
        d["raise"] = raise_
    else:
        d["ret"] = ret
    return d


def raise_spec(ty, msg=""):
    return {"type": ty, "message": msg}


# ── resolve_bedrock_bearer_token ────────────────────────────────────────────
add("resolve_bedrock_bearer_token", {"env": {"AWS_BEARER_TOKEN_BEDROCK": "  tok123  "}}, "tok123")
add("resolve_bedrock_bearer_token", {"env": {}}, "")
add("resolve_bedrock_bearer_token", {"env": {"AWS_BEARER_TOKEN_BEDROCK": ""}}, "")
add("resolve_bedrock_bearer_token",
    {"env": None, "io": {"import_get_hermes_home_override": None,
                         "get_hermes_home_override": None,
                         "environ_get": "  envtok  "}}, "envtok")
add("resolve_bedrock_bearer_token",
    {"env": None, "io": {"import_get_hermes_home_override": None,
                         "get_hermes_home_override": "/h/profile",
                         "import_get_secret": None,
                         "get_secret": " secret-tok "}}, "secret-tok")
add("resolve_bedrock_bearer_token",
    {"env": None, "io": {"import_get_hermes_home_override": script(raise_=raise_spec("ImportError", "no module"))}},
    {"__raise__": "ImportError", "message": "no module"})
add("resolve_bedrock_bearer_token", {"env": [1]},
    {"__raise__": "AttributeError", "message": "'list' object has no attribute 'get'"})
add("resolve_bedrock_bearer_token", {"env": {"AWS_BEARER_TOKEN_BEDROCK": 5}},
    {"__raise__": "AttributeError", "message": "'int' object has no attribute 'strip'"})

# ── cache_point_rejection_placement ─────────────────────────────────────────
def placement_case(msg, expect):
    add("cache_point_rejection_placement",
        {"exc": {"type": "ValidationException", "message": msg}}, expect)

placement_case("ValidationException: cachePoint is not permitted at #/system", "system")
placement_case("cachePoint not permitted at #/messages/0/content/1", "messages")
placement_case("extraneous cachePoint block at #/toolConfig/tools/0", "tools")
placement_case("cachePoint not permitted at #/unknownSection", "tools")
placement_case("cachePoint not permitted", "tools")
placement_case("some other error", None)
placement_case("cachePoint is fine here", None)
placement_case("CACHEPOINT NOT PERMITTED #/System", "system")
placement_case("extraneous cachePoint at #/", "tools")
placement_case("cachePoint not permitted at #/mytools[0]", "tools")

# ── strip_cache_points ──────────────────────────────────────────────────────
KW_MSG = {"messages": [{"role": "user", "content": [{"text": "hi"}, {"cachePoint": {}}]}]}
add("strip_cache_points", {"kwargs": KW_MSG, "placement": "messages"},
    {"same": False, "value": {"messages": [{"role": "user", "content": [{"text": "hi"}]}]}})
add("strip_cache_points",
    {"kwargs": {"messages": [{"content": [{"text": "x"}]}]}, "placement": "messages"},
    {"same": True, "value": {"messages": [{"content": [{"text": "x"}]}]}})
add("strip_cache_points", {"kwargs": {"modelId": "m"}, "placement": "messages"},
    {"same": True, "value": {"modelId": "m"}})
add("strip_cache_points",
    {"kwargs": {"system": [{"text": "s"}, {"cachePoint": {"ttl": "5m"}}]}, "placement": "system"},
    {"same": False, "value": {"system": [{"text": "s"}]}})
add("strip_cache_points", {"kwargs": {"system": "plain"}, "placement": "system"},
    {"same": True, "value": {"system": "plain"}})
add("strip_cache_points",
    {"kwargs": {"toolConfig": {"tools": [{"toolSpec": {"name": "t"}}, {"cachePoint": {}}]}},
     "placement": "tools"},
    {"same": False, "value": {"toolConfig": {"tools": [{"toolSpec": {"name": "t"}}]}}})
add("strip_cache_points", {"kwargs": {"toolConfig": None}, "placement": "tools"},
    {"same": True, "value": {"toolConfig": None}})
add("strip_cache_points", {"kwargs": {"a": 1}, "placement": "other"},
    {"same": True, "value": {"a": 1}})
add("strip_cache_points",
    {"kwargs": {"messages": ["x", {"content": [{"cachePoint": {}}]}]}, "placement": "messages"},
    {"same": False, "value": {"messages": ["x", {"content": []}]}})
add("strip_cache_points", {"kwargs": None, "placement": "system"},
    {"__raise__": "AttributeError", "message": "'NoneType' object has no attribute 'get'"})
add("strip_cache_points", {"kwargs": {"toolConfig": "x"}, "placement": "tools"},
    {"__raise__": "AttributeError", "message": "'str' object has no attribute 'get'"})
add("strip_cache_points",
    {"kwargs": {"messages": [{"content": [{"cachePoint": {}, "x": 1}]}]}, "placement": "messages"},
    {"same": True, "value": {"messages": [{"content": [{"cachePoint": {}, "x": 1}]}]}})

# ── recover_from_cache_point_rejection ──────────────────────────────────────
REC_IO = {"note_cache_point_rejection": None, "logger.warning": None}
add("recover_from_cache_point_rejection",
    {"exc": {"type": "ValidationException", "message": "cachePoint not permitted at #/system"},
     "kwargs": {"modelId": "m1", "system": [{"text": "s"}, {"cachePoint": {}}]}, "io": REC_IO},
    {"modelId": "m1", "system": [{"text": "s"}]})
add("recover_from_cache_point_rejection",
    {"exc": {"type": "X", "message": "unrelated"}, "kwargs": {"system": [{"cachePoint": {}}]}, "io": REC_IO},
    None)
add("recover_from_cache_point_rejection",
    {"exc": {"type": "X", "message": "cachePoint not permitted at #/system"},
     "kwargs": {"system": [{"text": "s"}]}, "io": REC_IO},
    None)
add("recover_from_cache_point_rejection",
    {"exc": {"type": "X", "message": "cachePoint not permitted at #/messages"},
     "kwargs": {"messages": [{"content": [{"cachePoint": {}}]}]}, "io": REC_IO},
    {"messages": [{"content": []}]})
add("recover_from_cache_point_rejection",
    {"exc": {"type": "X", "message": "cachePoint not permitted at #/system"},
     "kwargs": {"modelId": None, "system": [{"cachePoint": {}}]}, "io": REC_IO},
    {"modelId": None, "system": []})
add("recover_from_cache_point_rejection",
    {"exc": {"type": "X", "message": "cachePoint not permitted at #/system"},
     "kwargs": {"modelId": 42, "system": [{"cachePoint": {}}]}, "io": REC_IO},
    {"modelId": 42, "system": []})
add("recover_from_cache_point_rejection",
    {"exc": {"type": "X", "message": "cachePoint not permitted at #/system"},
     "kwargs": {"system": [{"cachePoint": {}}]},
     "io": {"note_cache_point_rejection": script(raise_=raise_spec("RuntimeError", "boom"))}},
    {"__raise__": "RuntimeError", "message": "boom"})

# ── prepare_send_message_platforms ──────────────────────────────────────────
add("prepare_send_message_platforms",
    {"io": {"import_discover_plugins": None, "discover_plugins": None}}, None)
add("prepare_send_message_platforms",
    {"io": {"import_discover_plugins": None,
            "discover_plugins": script(raise_=raise_spec("RuntimeError", "plug fail"))}},
    {"__raise__": "RuntimeError", "message": "plug fail"})
add("prepare_send_message_platforms",
    {"io": {"import_discover_plugins": script(raise_=raise_spec("ImportError", "no plugins"))}},
    {"__raise__": "ImportError", "message": "no plugins"})
add("prepare_send_message_platforms",
    {"io": {"import_discover_plugins": None, "discover_plugins": "ignored"}}, None)
add("prepare_send_message_platforms",
    {"io": {"import_discover_plugins": None,
            "discover_plugins": script(raise_=raise_spec("SystemExit", "bye"))}},
    {"__raise__": "SystemExit", "message": "bye"})
add("prepare_send_message_platforms", {"io": {"import_discover_plugins": None}},
    {"__raise__": "RuntimeError", "message": "no script for discover_plugins"})

# ── bounded_send_error ──────────────────────────────────────────────────────
add("bounded_send_error", {"detail": "short"}, "short")
add("bounded_send_error", {"detail": None}, "send failed")
add("bounded_send_error", {"detail": ""}, "send failed")
add("bounded_send_error", {"detail": 0}, "send failed")
add("bounded_send_error", {"detail": 5}, "5")
add("bounded_send_error", {"detail": True}, "True")
add("bounded_send_error", {"detail": False}, "send failed")
add("bounded_send_error", {"detail": "x" * 1000}, "x" * 897 + "...")
add("bounded_send_error", {"detail": "abcdef", "max_chars": 5}, "ab...")
add("bounded_send_error", {"detail": "abcdef", "max_chars": 3}, "...")
add("bounded_send_error", {"detail": "abcdef", "max_chars": 0}, "abc...")
add("bounded_send_error", {"detail": "abcdef", "max_chars": 2}, "abcde...")
add("bounded_send_error", {"detail": "héllo🌍w", "max_chars": 6}, "hél...")
add("bounded_send_error", {"detail": "héllo🌍w", "max_chars": 8}, "héllo🌍w")

# ── skills_guard pattern builders ───────────────────────────────────────────
for fn in ("shell_write_re", "prose_modify_re", "content_contract_re"):
    for fa in ("AGENTS.md", "CLAUDE.md", "a|b", "x(y)", "", "skills/foo/SKILL.md"):
        add(fn, {"file_alt": fa})  # expect filled from live during validation

# ── named_profile_for_key ───────────────────────────────────────────────────
def np_case(config, seam, expect, sk="agent:p1:main"):
    add("named_profile_for_key",
        {"self": {"config": config}, "session_key": sk,
         "io": {"_profile_from_session_key": seam}}, expect)

np_case({}, "p1", None)
np_case({"multiplex_profiles": True}, "p1", "p1")
np_case({"multiplex_profiles": True}, "default", None)
np_case({"multiplex_profiles": True}, "", None)
np_case({"multiplex_profiles": True}, None, None)
np_case(None, "p1", None)
np_case({"multiplex_profiles": True}, script(raise_=raise_spec("RuntimeError", "x")),
        {"__raise__": "RuntimeError", "message": "x"})
np_case({"multiplex_profiles": "yes"}, "p2", "p2")

# ── profile_home_for_key ────────────────────────────────────────────────────
def ph_case(cache, io, expect, expect_cache=None):
    inp = {"self": {"_profile_home_cache": cache}, "session_key": "agent:p1:main", "io": io}
    add("profile_home_for_key", inp, expect)
    CASES[-1]["_expect_cache"] = expect_cache

ph_case({}, {"_named_profile_for_key": None}, None, {})
ph_case({"p1": "/cached/home"}, {"_named_profile_for_key": "p1"}, "/cached/home",
        {"p1": "/cached/home"})
ph_case({}, {"_named_profile_for_key": "p1", "import_profiles": None,
             "profile_exists": True, "get_profile_dir": "/h/p1"}, "/h/p1", {"p1": "/h/p1"})
ph_case({}, {"_named_profile_for_key": "p1", "import_profiles": None,
             "profile_exists": False}, None, {})
ph_case({}, {"_named_profile_for_key": "p1",
             "import_profiles": script(raise_=raise_spec("ImportError", "no profiles")),
             "logger.debug": None}, None, {})
ph_case({}, {"_named_profile_for_key": "p1", "import_profiles": None,
             "profile_exists": True,
             "get_profile_dir": script(raise_=raise_spec("OSError", "denied")),
             "logger.debug": None}, None, {})
ph_case({}, {"_named_profile_for_key": "p1", "import_profiles": None,
             "profile_exists": True, "get_profile_dir": None,
             "logger.debug": None}, None, {})
ph_case({}, {"_named_profile_for_key": "p1", "import_profiles": None,
             "profile_exists": script(raise_=raise_spec("SystemExit", "stop"))},
        {"__raise__": "SystemExit", "message": "stop"}, {})

# ── owner_key_for_session_id ────────────────────────────────────────────────
add("owner_key_for_session_id", {"self": {"_entries": {}}, "session_id": None}, None)
add("owner_key_for_session_id",
    {"self": {"_entries": {"e1": {"session_id": "s1", "session_key": "k1"}}}, "session_id": "s1"}, "k1")
add("owner_key_for_session_id",
    {"self": {"_entries": {"e1": {"session_id": "sX", "session_key": "kX"}},
              "_session_owner_hints": {"s2": "k2"}}, "session_id": "s2"}, "k2")
add("owner_key_for_session_id",
    {"self": {"_entries": {"bad": {"session_key": "kb"},
                           "e1": {"session_id": "s1", "session_key": "k1"}},
              "_session_owner_hints": {"s1": "hint1"}}, "session_id": "s1"}, "hint1")
add("owner_key_for_session_id",
    {"self": {"_session_owner_hints": {"s1": "k1"}}, "session_id": "s1"}, "k1")
add("owner_key_for_session_id",
    {"self": {"_entries": {}, "_session_owner_hints": "x"}, "session_id": "s1"},
    {"__raise__": "AttributeError", "message": "'str' object has no attribute 'get'"})
add("owner_key_for_session_id",
    {"self": {"_entries": [1]}, "session_id": "s1"}, None)
add("owner_key_for_session_id",
    {"self": {"_entries": {"e1": {"session_id": 5, "session_key": "k5"}}}, "session_id": "5"}, None)

# ── get_transcript_drain_lock ───────────────────────────────────────────────
for r in ("lock-1", {"id": "L2"}, None, 42, script(raise_=raise_spec("RuntimeError", "no lock"))):
    expect = r if not (isinstance(r, dict) and r.get("__script__")) else {
        "__raise__": "RuntimeError", "message": "no lock"}
    add("get_transcript_drain_lock", {"self": {}, "io": {"_lazy": r}}, expect)

# ── normalize_archive_parts ─────────────────────────────────────────────────
def arch_case(name, expect):
    add("normalize_archive_parts", {"member_name": name}, expect)

def unsafe(name):
    return {"__raise__": "ValueError", "message": f"Unsafe archive member path: {name}"}

arch_case("pkg/sub/file.txt", ["pkg", "sub", "file.txt"])
arch_case("a\\b\\c", ["a", "b", "c"])
arch_case("/abs/path", unsafe("/abs/path"))
arch_case("C:\\x", unsafe("C:\\x"))
arch_case("C:rel", unsafe("C:rel"))
arch_case("../escape", unsafe("../escape"))
arch_case("a/../b", unsafe("a/../b"))
arch_case("", unsafe(""))
arch_case(".", unsafe("."))
arch_case("a//b/./c", ["a", "b", "c"])
arch_case("\\\\srv\\share\\x", unsafe("\\\\srv\\share\\x"))
arch_case("1:x", unsafe("1:x"))
arch_case("a:b", unsafe("a:b"))
arch_case("ab:c", ["ab:c"])
arch_case(None, {"__raise__": "AttributeError", "message": "'NoneType' object has no attribute 'replace'"})
arch_case("a/.../b", ["a", "...", "b"])
arch_case("a\\..\\b", unsafe("a\\..\\b"))
arch_case("pkg/", ["pkg"])
arch_case("é:x", unsafe("é:x"))
arch_case(" :x", unsafe(" :x"))

# ── normalize_windows_path ──────────────────────────────────────────────────
add("normalize_windows_path", {"value": "C:/Program Files/Hermes/"}, "c:\\program files\\hermes")
add("normalize_windows_path", {"value": "C:\\X\\"}, "c:\\x")
add("normalize_windows_path", {"value": ""}, "")
add("normalize_windows_path", {"value": None}, "none")
add("normalize_windows_path", {"value": 5}, "5")
add("normalize_windows_path", {"value": True}, "true")
add("normalize_windows_path", {"value": "a/b\\c/"}, "a\\b\\c")

# ── windows_user_path_entries ───────────────────────────────────────────────
def wpe_case(io, expect):
    add("windows_user_path_entries", {"io": io}, expect)

WPE_OK = {"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(["%USERPROFILE%\\bin;C:\\x;;  ;y", 2]),
          "os.path.expandvars": "C:\\Users\\u\\bin;C:\\x;;  ;y"}
wpe_case(WPE_OK, ["C:\\Users\\u\\bin", "C:\\x", "y"])
wpe_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(raise_=raise_spec("FileNotFoundError", "no Path")),
          "os.environ.get": "a;b"}, ["a", "b"])
wpe_case({"import_winreg": script(raise_=raise_spec("ImportError", "no winreg")),
          "os.environ.get": "a;b"}, ["a", "b"])
wpe_case({"import_winreg": None,
          "winreg.OpenKey": script(raise_=raise_spec("PermissionError", "denied")),
          "os.environ.get": "a;b"}, ["a", "b"])
wpe_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(["a;b", 2]),
          "os.path.expandvars": script(raise_=raise_spec("OSError", "bad var")),
          "os.environ.get": "fallback"}, ["fallback"])
wpe_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script([1])},
         {"__raise__": "ValueError", "message": "not enough values to unpack (expected 2, got 1)"})
wpe_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": None},
         {"__raise__": "TypeError", "message": "cannot unpack non-iterable NoneType object"})
wpe_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(["a", 2]),
          "os.path.expandvars": script(raise_=raise_spec("RuntimeError", "boom"))},
         {"__raise__": "RuntimeError", "message": "boom"})
wpe_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(["", 2]),
          "os.path.expandvars": "", "os.environ.get": "unused"}, [])

# ── read_user_path_raw ──────────────────────────────────────────────────────
def rup_case(io, expect):
    add("read_user_path_raw", {"io": io}, expect)

rup_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(["a;%X%;b", 2])}, [["a", "%X%", "b"], 2])
rup_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(raise_=raise_spec("FileNotFoundError", "missing"))},
         [[], 2])
rup_case({"import_winreg": script(raise_=raise_spec("ImportError", "no winreg"))},
         {"__raise__": "ImportError", "message": "no winreg"})
rup_case({"import_winreg": None,
          "winreg.OpenKey": script(raise_=raise_spec("OSError", "denied"))},
         {"__raise__": "OSError", "message": "denied"})
rup_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(["a", "1"])}, [["a"], 1])
rup_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(["a", "x"])},
         {"__raise__": "ValueError", "message": "invalid literal for int() with base 10: 'x'"})
rup_case({"import_winreg": None, "winreg.OpenKey": "hk1",
          "winreg.QueryValueEx": script(["a; ;b", 1])}, [["a", " ", "b"], 1])

# ── dashboard_client_heartbeat_path ─────────────────────────────────────────
REL = "state/dashboard_clients.heartbeat"
add("dashboard_client_heartbeat_path", {"hermes_home": "/h"}, "/h/" + REL)
add("dashboard_client_heartbeat_path",
    {"hermes_home": None, "io": {"import_get_process_hermes_home": None,
                                 "get_process_hermes_home": "/h2"}}, "/h2/" + REL)
add("dashboard_client_heartbeat_path", {"hermes_home": ""}, REL)
add("dashboard_client_heartbeat_path", {"hermes_home": "C:\\h\\"}, "C:/h/" + REL)
add("dashboard_client_heartbeat_path", {"hermes_home": 5},
    {"__raise__": "TypeError",
     "message": "argument should be a str or an os.PathLike object where __fspath__ returns a str, not 'int'"})
add("dashboard_client_heartbeat_path", {"hermes_home": "//srv/share"}, "//srv/share/" + REL)
add("dashboard_client_heartbeat_path", {"hermes_home": "h/./x"}, "h/x/" + REL)
add("dashboard_client_heartbeat_path",
    {"hermes_home": None, "io": {"import_get_process_hermes_home": None,
                                 "get_process_hermes_home": script(raise_=raise_spec("OSError", "no home"))}},
    {"__raise__": "OSError", "message": "no home"})
add("dashboard_client_heartbeat_path", {"hermes_home": "///h"}, "///h/" + REL)

# ── touch_dashboard_client_heartbeat ────────────────────────────────────────
TOUCH_OK = {"os.path.dirname": "/h/state", "os.makedirs": None, "open": "fd",
            "close": None, "os.utime": None, "logger.debug": None}
add("touch_dashboard_client_heartbeat", {"path": "/h/state/marker", "io": TOUCH_OK}, True)
add("touch_dashboard_client_heartbeat",
    {"path": None, "io": dict(TOUCH_OK, **{"dashboard_client_heartbeat_path": "/h/state/marker"})}, True)
add("touch_dashboard_client_heartbeat",
    {"path": "/m", "io": dict(TOUCH_OK, **{"os.makedirs": script(raise_=raise_spec("OSError", "ro"))})}, False)
add("touch_dashboard_client_heartbeat",
    {"path": "/m", "io": dict(TOUCH_OK, **{"open": script(raise_=raise_spec("PermissionError", "no"))})}, False)
add("touch_dashboard_client_heartbeat",
    {"path": "/m", "io": dict(TOUCH_OK, **{"os.utime": script(raise_=raise_spec("OSError", "utime"))})}, False)
add("touch_dashboard_client_heartbeat",
    {"path": "/m", "io": dict(TOUCH_OK, **{"close": script(raise_=raise_spec("RuntimeError", "close"))})}, False)
add("touch_dashboard_client_heartbeat",
    {"path": "/m", "io": dict(TOUCH_OK, **{"os.makedirs": script(raise_=raise_spec("SystemExit", "bye"))})},
    {"__raise__": "SystemExit", "message": "bye"})
add("touch_dashboard_client_heartbeat",
    {"path": "/m", "io": dict(TOUCH_OK, **{"os.path.dirname": script(raise_=raise_spec("OSError", "dn"))})}, False)

# ── dashboard_client_last_seen ──────────────────────────────────────────────
add("dashboard_client_last_seen",
    {"path": "/m", "now": 1000, "io": {"os.stat": {"st_mtime": 500.5}}}, 500.5)
add("dashboard_client_last_seen",
    {"path": "/m", "now": None, "io": {"time.time": 2000, "os.stat": {"st_mtime": 3000}}}, 2000)
add("dashboard_client_last_seen",
    {"path": "/m", "now": 1000,
     "io": {"os.stat": script(raise_=raise_spec("FileNotFoundError", "gone"))}}, None)
add("dashboard_client_last_seen",
    {"path": "/m", "now": 1000,
     "io": {"os.stat": script(raise_=raise_spec("PermissionError", "no"))}}, 1000)
add("dashboard_client_last_seen",
    {"path": None, "now": 1000,
     "io": {"dashboard_client_heartbeat_path": "/m", "os.stat": {"st_mtime": 42}}}, 42)
add("dashboard_client_last_seen",
    {"path": "/m", "now": 1000,
     "io": {"os.stat": script(raise_=raise_spec("RuntimeError", "boom"))}},
    {"__raise__": "RuntimeError", "message": "boom"})
add("dashboard_client_last_seen",
    {"path": "/m", "now": 0, "io": {"os.stat": {"st_mtime": 5}}}, 0)
add("dashboard_client_last_seen",
    {"path": "/m", "now": 1000, "io": {"os.stat": {}}},
    {"__raise__": "AttributeError", "message": "'types.SimpleNamespace' object has no attribute 'st_mtime'"})

# ── start_attestation_path ──────────────────────────────────────────────────
AREL = "state/gateway.start-attestation.json"
add("start_attestation_path", {"home": "/h"}, "/h/" + AREL)
add("start_attestation_path", {"home": None, "io": {"_hermes_home": "/h2"}}, "/h2/" + AREL)
add("start_attestation_path", {"home": ""}, AREL)
add("start_attestation_path", {"home": "C:\\h"}, "C:/h/" + AREL)
add("start_attestation_path", {"home": 5},
    {"__raise__": "AttributeError", "message": "'int' object has no attribute 'joinpath'"})
add("start_attestation_path",
    {"home": None, "io": {"_hermes_home": script(raise_=raise_spec("OSError", "no home"))}},
    {"__raise__": "OSError", "message": "no home"})

# ── macos_protected_search_warning ──────────────────────────────────────────
PRE = ("Skipped macOS protected folders during broad search to avoid "
       "an unattended privacy prompt: ")
POST = ". Search a protected folder directly when access is intentional."
add("macos_protected_search_warning",
    {"paths": ["/Users/x/Library/Mail", "/Users/x/Library/Messages"]}, PRE + "Mail, Messages" + POST)
add("macos_protected_search_warning", {"paths": ["C:\\x\\y", "a/b/", "plain"]}, PRE + "y, , plain" + POST)
add("macos_protected_search_warning", {"paths": []}, PRE + "" + POST)
add("macos_protected_search_warning", {"paths": ["C:", "1:x", "é:x"]}, PRE + ", x, x" + POST)
add("macos_protected_search_warning", {"paths": None},
    {"__raise__": "TypeError", "message": "'NoneType' object is not iterable"})
add("macos_protected_search_warning",
    {"paths": ["\\\\srv\\share\\f", "//srv/share/g"]}, PRE + "f, g" + POST)
add("macos_protected_search_warning", {"paths": ["a/.", "..", "x\\"]}, PRE + "., .., " + POST)

# ── pxb_name / pxb_display_name ─────────────────────────────────────────────
for i in range(5):
    add("pxb_name", {"self": {"i": i}}, None)
    add("pxb_display_name", {"self": {"i": i}}, None)

# ── line_input ──────────────────────────────────────────────────────────────
LI_TTY = {"stdin.isatty": True, "stdout.isatty": True,
          "import_prompt_toolkit_prompt": None, "import_ANSI": None}
add("line_input", {"prompt_text": "P> ", "io": {"stdin.isatty": False, "input": "ans"}}, "ans")
add("line_input", {"prompt_text": "P> ", "io": {"stdin.isatty": True, "stdout.isatty": False, "input": "ans"}}, "ans")
add("line_input", {"prompt_text": "P> ", "io": dict(LI_TTY, **{"prompt_toolkit_prompt": "typed"})}, "typed")
add("line_input", {"prompt_text": "P> ",
                   "io": {"stdin.isatty": True, "stdout.isatty": True,
                          "import_prompt_toolkit_prompt": script(raise_=raise_spec("ImportError", "no pt")),
                          "input": "fallback"}}, "fallback")
add("line_input", {"prompt_text": "P> ",
                   "io": {"stdin.isatty": True, "stdout.isatty": True,
                          "import_prompt_toolkit_prompt": None,
                          "import_ANSI": script(raise_=raise_spec("ImportError", "no ansi")),
                          "input": "fallback"}}, "fallback")
add("line_input", {"prompt_text": "P> ",
                   "io": dict(LI_TTY, **{"prompt_toolkit_prompt": script(raise_=raise_spec("KeyboardInterrupt", ""))})},
    {"__raise__": "KeyboardInterrupt", "message": ""})
add("line_input", {"prompt_text": "P> ",
                   "io": dict(LI_TTY, **{"prompt_toolkit_prompt": script(raise_=raise_spec("EOFError", ""))})},
    {"__raise__": "EOFError", "message": ""})
add("line_input", {"prompt_text": "P> ",
                   "io": dict(LI_TTY, **{"prompt_toolkit_prompt": script(raise_=raise_spec("RuntimeError", "sel")),
                                         "input": "fallback"})}, "fallback")
add("line_input", {"prompt_text": "P> ",
                   "io": dict(LI_TTY, **{"prompt_toolkit_prompt": script(raise_=raise_spec("SystemExit", "x"))})},
    {"__raise__": "SystemExit", "message": "x"})
add("line_input", {"prompt_text": "P> ",
                   "io": {"stdin.isatty": False, "input": script(raise_=raise_spec("EOFError", ""))}},
    {"__raise__": "EOFError", "message": ""})

# ── user_enabled ────────────────────────────────────────────────────────────
def ue_case(setting, default, io, expect):
    add("user_enabled", {"setting": setting, "default": default, "io": io}, expect)

UE_IMP = {"import_load_config_readonly": None}
ue_case("x", True, dict(UE_IMP, **{"load_config_readonly": {"display": {"x": True}}}), True)
ue_case("x", True, dict(UE_IMP, **{"load_config_readonly": {"display": {"x": 0}}}), False)
ue_case("x", False, dict(UE_IMP, **{"load_config_readonly": {"display": {"x": "yes"}}}), True)
ue_case("x", True, dict(UE_IMP, **{"load_config_readonly": {}}), True)
ue_case("x", True, dict(UE_IMP, **{"load_config_readonly": {"display": "nope"}}), True)
ue_case("x", True, dict(UE_IMP, **{"load_config_readonly": script(raise_=raise_spec("RuntimeError", "cfg"))}), True)
ue_case("x", False, {"import_load_config_readonly": script(raise_=raise_spec("ImportError", "no cfg"))}, False)
ue_case("x", True, dict(UE_IMP, **{"load_config_readonly": script(raise_=raise_spec("SystemExit", "bye"))}),
        {"__raise__": "SystemExit", "message": "bye"})
ue_case("x", True, dict(UE_IMP, **{"load_config_readonly": {"display": {"x": None}}}), False)
ue_case("y", True, dict(UE_IMP, **{"load_config_readonly": {"display": {"x": True}}}), True)

# ── has_explicit_vertex_config ──────────────────────────────────────────────
def va_case(io, expect):
    add("has_explicit_vertex_config", {"io": io}, expect)

va_case({"_resolve_project_override": "proj"}, True)
va_case({"_resolve_project_override": None, "_get_secret": "/sa.json",
         "os.path.isfile": True, "os.access": True}, True)
va_case({"_resolve_project_override": None, "_get_secret": ""}, False)
va_case({"_resolve_project_override": None, "_get_secret": "/sa.json", "os.path.isfile": False}, False)
va_case({"_resolve_project_override": None, "_get_secret": "/sa.json",
         "os.path.isfile": True, "os.access": False}, False)
va_case({"_resolve_project_override": script(raise_=raise_spec("RuntimeError", "ov"))},
        {"__raise__": "RuntimeError", "message": "ov"})
va_case({"_resolve_project_override": None, "_get_secret": None}, False)
va_case({"_resolve_project_override": None, "_get_secret": "/sa.json",
         "os.path.isfile": script(raise_=raise_spec("OSError", "stat"))},
        {"__raise__": "OSError", "message": "stat"})

# ── forget_task ─────────────────────────────────────────────────────────────
def ft_case(reads, lw, tid, expect_reads, expect_lw, expect_ret=None):
    add("forget_task", {"self": {"_reads": reads, "_last_writer": lw}, "task_id": tid}, expect_ret)
    CASES[-1]["_expect_reads"] = expect_reads
    CASES[-1]["_expect_lw"] = expect_lw

ft_case({"t1": [1, 2, 3], "t2": [4]}, {"/a": ["t1", 1], "/b": ["t2", 2], "/c": ["t1", 3]},
        "t1", {"t2": [4]}, {"/b": ["t2", 2]})
ft_case({"t1": [1]}, {}, "t1", {}, {})
ft_case({"t1": [1]}, {"/a": ["t2", 1]}, "t9", {"t1": [1]}, {"/a": ["t2", 1]})
ft_case({"t1": [1], "t2": [2]}, {"/a": [1]}, "t1", {"t2": [2]}, {"/a": [1]},
        {"__raise__": "ValueError", "message": "not enough values to unpack (expected 2, got 1)"})
ft_case({"t1": [1]}, {"/a": None}, "t1", {}, {"/a": None},
        {"__raise__": "TypeError", "message": "cannot unpack non-iterable NoneType object"})
ft_case({}, {"/a": ["t1", 1], "/b": ["t1", 2]}, "t1", {}, {})
ft_case({"t1": [1]}, {"/a": [5, 1]}, "5", {"t1": [1]}, {"/a": [5, 1]})

# ── get_loop_tick_socket_path ───────────────────────────────────────────────
def lt_case(home, pid, io, expect):
    add("get_loop_tick_socket_path", {"home": home, "pid": pid, "io": io}, expect)

lt_case("/h", 123, {}, "/h/state/gateway.loop-tick.123.sock")
lt_case(None, None, {"os.getpid": 456, "_process_hermes_home": "/h2"},
        "/h2/state/gateway.loop-tick.456.sock")
lt_case("/h", 3.9, {}, "/h/state/gateway.loop-tick.3.sock")
lt_case("/h", -3.9, {}, "/h/state/gateway.loop-tick.-3.sock")
lt_case("/h", " 42 ", {}, "/h/state/gateway.loop-tick.42.sock")
lt_case("/h", "1_0", {}, "/h/state/gateway.loop-tick.10.sock")
lt_case("/h", "abc", {},
        {"__raise__": "ValueError", "message": "invalid literal for int() with base 10: 'abc'"})
lt_case("", 123, {}, "state/gateway.loop-tick.123.sock")
lt_case(5, 123, {},
    {"__raise__": "TypeError", "message": "unsupported operand type(s) for /: 'int' and 'str'"})
lt_case("/h", True, {}, "/h/state/gateway.loop-tick.1.sock")

# ── get_terminal_scope ──────────────────────────────────────────────────────
def ts_case(state, io, expect):
    add("get_terminal_scope", {"io": dict(io, **{"current_state": state})}, expect)

TS_IMP = {"import_current_state": None}
ts_case({"terminal": {"mode": "ask"}}, TS_IMP, {"mode": "ask"})
ts_case({"terminal": {"__type__": "DeferredTerminalPolicy"}},
        dict(TS_IMP, **{"resolve": {"mode": "allow"}}), {"mode": "allow"})
ts_case({"terminal": {"__type__": "DeferredTerminalPolicy"}},
        dict(TS_IMP, **{"resolve": script(raise_=raise_spec("RuntimeError", "res"))}),
        {"__raise__": "RuntimeError", "message": "res"})
add("get_terminal_scope",
    {"io": dict(TS_IMP, **{"current_state": script(raise_=raise_spec("RuntimeError", "cs"))})},
    {"__raise__": "RuntimeError", "message": "cs"})
ts_case(None, {"import_current_state": script(raise_=raise_spec("ImportError", "no scope"))},
        {"__raise__": "ImportError", "message": "no scope"})
ts_case({}, TS_IMP,
        {"__raise__": "AttributeError", "message": "'types.SimpleNamespace' object has no attribute 'terminal'"})
ts_case({"terminal": None}, TS_IMP, None)


# ── validation + emit ───────────────────────────────────────────────────────

def load_oracle():
    spec = importlib.util.spec_from_file_location(
        "oracle_rest_revive1", REPO / "tests" / "oracle_rest_revive1.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules["oracle_rest_revive1"] = mod
    spec.loader.exec_module(mod)
    return mod


def main():
    validate_only = "--validate-only" in sys.argv
    orc = load_oracle()
    # Fill pattern-builder expectations from the live reference.
    for c in CASES:
        if c["func"] in ("shell_write_re", "prose_modify_re", "content_contract_re") and c["_expect"] is None:
            live = {"shell_write_re": "_shell_write_re", "prose_modify_re": "_prose_modify_re",
                    "content_contract_re": "_content_contract_re"}[c["func"]]
            c["_expect"] = orc.NS_SG[live](c["in"].get("file_alt", ""))
    failures = 0
    counts = {}
    for i, c in enumerate(CASES):
        func, inp, expect = c["func"], c["in"], c["_expect"]
        counts[func] = counts.get(func, 0) + 1
        driver = orc.DRIVERS[func]
        out = orc.run_case(driver, inp)
        if expect is not None and out["ret"] != orc.norm(expect):
            failures += 1
            print(f"EXPECT FAIL #{i} {func}: got {json.dumps(out['ret'], ensure_ascii=False)[:200]} "
                  f"want {json.dumps(orc.norm(expect), ensure_ascii=False)[:200]}")
        if "_expect_cache" in c:
            got_cache = out["state"].get("self", {}).get("_profile_home_cache")
            if got_cache != orc.norm(c["_expect_cache"]):
                failures += 1
                print(f"CACHE FAIL #{i} {func}: got {got_cache} want {c['_expect_cache']}")
        if "_expect_reads" in c:
            st = out["state"].get("self", {})
            if st.get("_reads") != orc.norm(c["_expect_reads"]) or st.get("_last_writer") != orc.norm(c["_expect_lw"]):
                failures += 1
                print(f"STATE FAIL #{i} {func}: got {st} want reads={c['_expect_reads']} lw={c['_expect_lw']}")
        # Trace sanity: every seam event has ev/args/ret keys.
        for ev in out["trace"]:
            if set(ev.keys()) != {"ev", "args", "ret"}:
                failures += 1
                print(f"TRACE SHAPE FAIL #{i} {func}: {ev}")
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        failures += 1
        print("TOO FEW CASES:", short)
    print(f"validated {len(CASES)} cases across {len(counts)} funcs; failures={failures}")
    if failures:
        sys.exit(1)
    if not validate_only:
        FIXTURE.parent.mkdir(parents=True, exist_ok=True)
        with open(FIXTURE, "w", encoding="utf-8", newline="\n") as f:
            for c in CASES:
                f.write(json.dumps({"func": c["func"], "in": c["in"]}, ensure_ascii=False) + "\n")
        print(f"wrote {FIXTURE} ({len(CASES)} lines)")


if __name__ == "__main__":
    main()
