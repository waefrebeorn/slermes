#!/usr/bin/env python3
"""Generate tests/fixtures/web_profiles_flow3.jsonl — scripted-seam FLOW cases
for the unclaimed async-endpoint + sidebar-cache port src/cli/web_profiles_flow3.c.

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises a REAL builtin of that class in the
live drive (full builtin hierarchy incl. the BaseException trio / groups) and is
delivered as a raise descriptor to the C universal seam. HTTPException carries
status_code+detail; ProfileIdentitySettlementPending carries path+retry_command.
A MISSING seam key fail-closes (RuntimeError) on BOTH sides; a present JSON null
is a valid None.

Run: python tests/gen_web_profiles_flow3_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "web_profiles_flow3.jsonl")

CASES = []


def add(func, inp, seams=None):
    inp = dict(inp)
    sc = dict(seams or {})
    sc.setdefault("log_exception", None)  # every _log.exception is a recorded no-op
    inp["seams"] = sc
    CASES.append({"func": func, "in": inp})


def rs(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def http(status, detail):
    return {"__raise__": "HTTPException", "status_code": status, "detail": detail}


def pending(path="/home/p", retry="hermes profile use p"):
    return {"__raise__": "ProfileIdentitySettlementPending", "path": path, "retry_command": retry}


# ══════════════ _profile_targets ══════════════
TAR = [["default", "/d"], ["p", "/p"]]
add("profile_targets", {"log_label": "GET /api/profiles/sessions"},
    {"profile_targets": {"__arr__": TAR}, "scope_to_launch_profile": {"__arr__": TAR}})
add("profile_targets", {"log_label": "sidebar"},
    {"profile_targets": {"__arr__": []}, "get_profile_dir": "/d/default",
     "scope_to_launch_profile": {"__arr__": [["default", "/d/default"]]}})
add("profile_targets", {"log_label": "scan"},
    {"profile_targets": rs("OSError"), "log_exception": None,
     "get_profile_dir": "/d/default", "scope_to_launch_profile": {"__arr__": [["default", "/d/default"]]}})
add("profile_targets", {"log_label": "scan"},
    {"profile_targets": rs("KeyboardInterrupt")})
add("profile_targets", {"log_label": "scan"},
    {"profile_targets": {"__arr__": TAR}, "scope_to_launch_profile": {"__arr__": [["p", "/p"]]}})
add("profile_targets", {"log_label": "scan"},
    {"profile_targets": {"__arr__": []}, "get_profile_dir": rs("ValueError")})

# ══════════════ _sidebar_profile_cache_put ══════════════
K1 = ["/h/p/db", ["dev", 1, 100], 20, [], 50, 100, [], False]
K_OLD = ["/h/p/db", ["dev", 0, 90], 20, [], 50, 100, [], False]
K_OTHER = ["/h/q/db", ["dev", 1, 100], 20, [], 50, 100, [], False]
V1 = {"recents": [{"id": 1}], "usage": {"tokens": 5}}
add("sidebar_cache_put", {"key": K1, "value": V1, "cache": [], "max_entries": 256})
add("sidebar_cache_put", {"key": K1, "value": V1,
                          "cache": [[K_OLD, {"recents": []}]], "max_entries": 256})
add("sidebar_cache_put", {"key": K1, "value": V1,
                          "cache": [[K1, {"old": True}], [K_OTHER, {"recents": [1]}]], "max_entries": 256})
add("sidebar_cache_put", {"key": K1, "value": V1,
                          "cache": [[K_OTHER, {"recents": [1]}], [K1, {"old": True}]], "max_entries": 256})
add("sidebar_cache_put", {"key": K1, "value": V1,
                          "cache": [[K_OTHER, {"recents": [1]}], [[K_OTHER, ["x", 0], 1, [], 1, 1, [], False], {"z": 1}]],
                          "max_entries": 2})
add("sidebar_cache_put", {"key": ["/h/z/db", ["dev", 2, 200], 5, [], 5, 5, [], False], "value": V1,
                          "cache": [[K_OTHER, {"recents": [1]}]], "max_entries": 3})
add("sidebar_cache_put", {"key": K1, "value": {"recents": [], "usage": {"tokens": 0}},
                          "cache": [[K_OLD, {"stale": 1}], [K_OTHER, {"recents": [1]}], [K1, {"old": 2}]],
                          "max_entries": 2})

# ══════════════ _sidebar_profile_cache_clear ══════════════
for cache in [[], [[K1, V1]], [[K1, V1], [K_OTHER, {"recents": [1]}]],
              [[K_OLD, {"recents": []}]], [[K_OTHER, {"usage": {"tokens": 9}}]]]:
    add("sidebar_cache_clear", {"cache": cache})

# ══════════════ _profile_errors (FULL builtin exception hierarchy) ══════════════
add("profile_errors", {"raise": rs("ValueError", "bad value"), "log_msg": "PATCH failed",
                       "args": ["p"], "not_found": ["FileNotFoundError"],
                       "bad_request": ["ValueError", "FileExistsError"]})
add("profile_errors", {"raise": rs("FileExistsError", "exists"), "log_msg": "PATCH failed",
                       "args": ["p"], "not_found": ["FileNotFoundError"],
                       "bad_request": ["ValueError", "FileExistsError"]})
add("profile_errors", {"raise": rs("FileNotFoundError", "no dir"), "log_msg": "DELETE failed",
                       "args": ["p"], "not_found": ["FileNotFoundError"], "bad_request": []},
    {"log_exception": None})
add("profile_errors", {"raise": http(422, "custom"), "log_msg": "GET failed", "args": [],
                       "not_found": [], "bad_request": []})
add("profile_errors", {"raise": pending(), "log_msg": "DELETE failed", "args": ["p"],
                       "not_found": [], "bad_request": []})
add("profile_errors", {"raise": rs("RuntimeError", "boom"), "log_msg": "POST failed",
                       "args": ["a"], "not_found": [], "bad_request": []},
    {"log_exception": None})
add("profile_errors", {"raise": rs("PermissionError", "denied"), "log_msg": "POST failed",
                       "args": [], "not_found": ["FileNotFoundError"], "bad_request": ["ValueError"]},
    {"log_exception": None})
add("profile_errors", {"raise": rs("KeyboardInterrupt"), "log_msg": "POST failed",
                       "args": [], "not_found": [], "bad_request": []})
add("profile_errors", {"raise": rs("SystemExit"), "log_msg": "POST failed",
                       "args": [], "not_found": [], "bad_request": []})
add("profile_errors", {"raise": rs("GeneratorExit"), "log_msg": "POST failed",
                       "args": [], "not_found": [], "bad_request": []})
add("profile_errors", {"raise": rs("BaseExceptionGroup", "group"), "log_msg": "POST failed",
                       "args": [], "not_found": [], "bad_request": []})
add("profile_errors", {"raise": rs("ExceptionGroup", "group"), "log_msg": "POST failed",
                       "args": [], "not_found": [], "bad_request": []}, {"log_exception": None})
add("profile_errors", {"raise": rs("JSONDecodeError", "bad json"), "log_msg": "GET failed",
                       "args": [], "not_found": [], "bad_request": ["ValueError"]})
add("profile_errors", {"raise": rs("UnicodeDecodeError", "bad utf8"), "log_msg": "GET failed",
                       "args": [], "not_found": [], "bad_request": ["UnicodeError"]})
add("profile_errors", {"raise": rs("ZeroDivisionError", "zero"), "log_msg": "POST failed",
                       "args": [], "not_found": [], "bad_request": []}, {"log_exception": None})

# ══════════════ _read_off_loop ══════════════
add("read_off_loop", {"label": "SOUL.md", "errors": ["OSError"]}, {"read_result": "hello"})
add("read_off_loop", {"label": "SOUL.md", "errors": ["OSError"]}, {"read_result": rs("OSError", "denied")})
add("read_off_loop", {"label": "desktop.json", "errors": ["Exception"]}, {"read_result": rs("ValueError", "bad")})
add("read_off_loop", {"label": "SOUL.md", "errors": ["OSError"]}, {"read_result": rs("TypeError", "oops")})
add("read_off_loop", {"label": "SOUL.md", "errors": ["OSError"]}, {"read_result": rs("KeyboardInterrupt")})
add("read_off_loop", {"label": "SOUL.md", "errors": ["OSError"]}, {"read_result": None})
add("read_off_loop", {"label": "db", "errors": ["OSError", "ValueError"]}, {"read_result": {"rows": [1, 2]}})

# ══════════════ get_profile_setup_command ══════════════
for name in ["default", "p", "", "  p  ", "work-1"]:
    add("profile_setup_command", {"name": name}, {"resolve_profile_dir": "/h/" + name.strip()})

# ══════════════ get_active_profile_endpoint ══════════════
add("active_profile", {}, {"get_active_profile": "work", "get_active_profile_name": "work"})
add("active_profile", {}, {"get_active_profile": None, "get_active_profile_name": ""})
add("active_profile", {}, {"get_active_profile": rs("OSError"), "get_active_profile_name": "custom2"})
add("active_profile", {}, {"get_active_profile": "p", "get_active_profile_name": rs("ValueError")})
add("active_profile", {}, {"get_active_profile": False, "get_active_profile_name": "default"})
add("active_profile", {}, {"get_active_profile": rs("KeyboardInterrupt"), "get_active_profile_name": "x"})

# ══════════════ set_active_profile_endpoint ══════════════
add("set_active_profile", {"name": "p"}, {"set_active_profile": None, "normalize_profile_name": "p"})
add("set_active_profile", {"name": "p"}, {"set_active_profile": rs("FileNotFoundError")},)
add("set_active_profile", {"name": "p"}, {"set_active_profile": rs("ValueError", "bad name")},)
add("set_active_profile", {"name": "p"}, {"set_active_profile": rs("OSError", "disk")},)
add("set_active_profile", {"name": "p"}, {"set_active_profile": http(422, "custom")})
add("set_active_profile", {"name": "p"}, {"set_active_profile": None, "normalize_profile_name": rs("ValueError")})

# ══════════════ rename_profile_endpoint ══════════════
add("rename_profile", {"name": "p", "new_name": "q"},
    {"rename_profile": "/h/q", "normalize_profile_name": ["p", "q"]})
add("rename_profile", {"name": "default", "new_name": "  Main  "},
    {"rename_profile": "/h/default", "normalize_profile_name": ["default", "Main"]})
add("rename_profile", {"name": "p", "new_name": "q"}, {"rename_profile": rs("FileNotFoundError")})
add("rename_profile", {"name": "p", "new_name": "q"}, {"rename_profile": rs("FileExistsError", "taken")})
add("rename_profile", {"name": "p", "new_name": "q"},
    {"rename_profile": "/h/q", "normalize_profile_name": [rs("ValueError"), "q"]})
add("rename_profile", {"name": "p", "new_name": "q"},
    {"rename_profile": "/h/q", "normalize_profile_name": ["p", rs("TypeError")]})
add("rename_profile", {"name": "p", "new_name": "q"}, {"rename_profile": rs("OSError", "disk")})

# ══════════════ delete_profile_endpoint ══════════════
add("delete_profile", {"name": "p"}, {"delete_profile": "/h/p"})
add("delete_profile", {"name": "p"}, {"delete_profile": pending()})
add("delete_profile", {"name": "p"}, {"delete_profile": rs("FileNotFoundError")})
add("delete_profile", {"name": "p"}, {"delete_profile": rs("OSError", "busy")})
add("delete_profile", {"name": "p"}, {"delete_profile": http(403, "forbidden")})
add("delete_profile", {"name": "p"}, {"delete_profile": rs("ValueError", "bad")})

# ══════════════ get_profile_soul ══════════════
add("profile_soul", {"name": "p"}, {"resolve_profile_dir": "/h/p", "soul_exists": False})
add("profile_soul", {"name": "p"}, {"resolve_profile_dir": "/h/p", "soul_exists": True,
                                   "soul_read_text": "soul content"})
add("profile_soul", {"name": "p"}, {"resolve_profile_dir": "/h/p", "soul_exists": rs("OSError", "stat")})
add("profile_soul", {"name": "p"}, {"resolve_profile_dir": "/h/p", "soul_exists": rs("ValueError", "weird")})
add("profile_soul", {"name": "p"}, {"resolve_profile_dir": "/h/p", "soul_exists": True,
                                   "soul_read_text": rs("OSError", "read")})
add("profile_soul", {"name": "p"}, {"resolve_profile_dir": rs("RuntimeError", "resolve")})
add("profile_soul", {"name": "p"}, {"resolve_profile_dir": "/h/p", "soul_exists": True, "soul_read_text": ""})

# ══════════════ update_profile_soul ══════════════
add("update_soul", {"name": "p", "content": "new soul"}, {"resolve_profile_dir": "/h/p", "atomic_write_text": None})
add("update_soul", {"name": "p", "content": "x"}, {"resolve_profile_dir": "/h/p", "atomic_write_text": rs("OSError", "full")})
add("update_soul", {"name": "p", "content": "x"}, {"resolve_profile_dir": "/h/p", "atomic_write_text": rs("FileNotFoundError", "dir")})
add("update_soul", {"name": "p", "content": "x"}, {"resolve_profile_dir": "/h/p", "atomic_write_text": rs("ValueError", "bad")})
add("update_soul", {"name": "p", "content": "x"}, {"resolve_profile_dir": "/h/p", "atomic_write_text": rs("KeyboardInterrupt")})
add("update_soul", {"name": "p", "content": ""}, {"resolve_profile_dir": "/h/p", "atomic_write_text": None})

# ══════════════ update_profile_model_endpoint ══════════════
add("update_model", {"name": "p", "provider": " openai ", "model": " gpt-4 "},
    {"resolve_profile_dir": "/h/p", "write_profile_model": None})
add("update_model", {"name": "p", "provider": "", "model": "gpt"}, {"resolve_profile_dir": "/h/p"})
add("update_model", {"name": "p", "provider": "x", "model": "  "}, {"resolve_profile_dir": "/h/p"})
add("update_model", {"name": "p", "provider": "x", "model": "m"},
    {"resolve_profile_dir": "/h/p", "write_profile_model": rs("ModelAssignmentRejected", "no")})
add("update_model", {"name": "p", "provider": "x", "model": "m"},
    {"resolve_profile_dir": "/h/p", "write_profile_model": rs("RuntimeError", "disk")})
add("update_model", {"name": "p", "provider": "x", "model": "m"},
    {"resolve_profile_dir": rs("ValueError", "resolve")})
add("update_model", {"name": "p", "provider": "  x  ", "model": "  m  "},
    {"resolve_profile_dir": "/h/p", "write_profile_model": None})

# ══════════════ describe_profile_auto_endpoint ══════════════
add("describe_auto", {"name": "p", "overwrite": True},
    {"resolve_profile_dir": "/h/p",
     "describe_profile": {"ok": True, "reason": "", "description": "auto desc"}})
add("describe_auto", {"name": "p", "overwrite": False},
    {"resolve_profile_dir": "/h/p",
     "describe_profile": {"ok": False, "reason": "no model", "description": None}})
add("describe_auto", {"name": "p", "overwrite": False},
    {"resolve_profile_dir": "/h/p", "describe_profile": rs("RuntimeError", "llm")})
add("describe_auto", {"name": "p", "overwrite": True},
    {"resolve_profile_dir": "/h/p", "describe_profile": rs("ValueError", "bad")})
add("describe_auto", {"name": "p", "overwrite": False},
    {"resolve_profile_dir": "/h/p",
     "describe_profile": {"ok": True, "reason": None, "description": "d"}})
add("describe_auto", {"name": "p", "overwrite": False}, {"resolve_profile_dir": rs("FileNotFoundError")})

# ══════════════ create_profile_endpoint ══════════════
add("create_profile", {"body": {"name": "p", "clone_from": None, "clone_all": False,
                                "clone_from_default": False, "provider": " openai ", "model": " gpt ",
                                "hub_skills": [], "soul": None, "refusals": []}},
    {"provision_profile": {"path": "/h/p", "model_set": True, "model_error": None,
                           "mcp_written": False, "skills_disabled": [], "warnings": []}})
add("create_profile", {"body": {"name": "q", "clone_from": "  src  ", "clone_all": True,
                                "clone_from_default": False, "provider": "", "model": "",
                                "hub_skills": [" a ", "", "b"], "soul": "text", "refusals": []}},
    {"provision_profile": {"path": "/h/q", "model_set": False, "model_error": "x",
                           "mcp_written": True, "skills_disabled": ["s"], "warnings": ["w"]},
     "spawn_install": [123, 456]})
add("create_profile", {"body": {"name": "r", "clone_from": None, "clone_all": True,
                                "clone_from_default": False, "provider": "p", "model": "m",
                                "hub_skills": [], "soul": None, "refusals": []}},
    {"provision_profile": {"path": "/h/r", "model_set": None, "model_error": None,
                           "mcp_written": None, "skills_disabled": None, "warnings": None}})
add("create_profile", {"body": {"name": "s", "clone_from": None, "clone_all": False,
                                "clone_from_default": True, "provider": "p", "model": "m",
                                "hub_skills": [], "soul": None, "refusals": ["ValueError"]}},
    {"provision_profile": rs("ValueError", "refused")})
add("create_profile", {"body": {"name": "t", "clone_from": None, "clone_all": False,
                                "clone_from_default": False, "provider": "p", "model": "m",
                                "hub_skills": [], "soul": None, "refusals": []}},
    {"provision_profile": rs("OSError", "disk")})
add("create_profile", {"body": {"name": "u", "clone_from": None, "clone_all": False,
                                "clone_from_default": False, "provider": "p", "model": "m",
                                "hub_skills": ["x"], "soul": None, "refusals": []}},
    {"provision_profile": {"path": "/h/u", "model_set": True, "model_error": None,
                           "mcp_written": None, "skills_disabled": [], "warnings": []},
     "spawn_install": rs("ValueError", "spawn"), "log_exception": None})
add("create_profile", {"body": {"name": "v", "clone_from": None, "clone_all": False,
                                "clone_from_default": False, "provider": "p", "model": "m",
                                "hub_skills": ["x"], "soul": None, "refusals": []}},
    {"provision_profile": {"path": "/h/v", "model_set": True, "model_error": None,
                           "mcp_written": None, "skills_disabled": [], "warnings": []},
     "spawn_install": rs("KeyboardInterrupt")})
add("create_profile", {"body": {"name": "w", "clone_from": "  ", "clone_all": False,
                                "clone_from_default": False, "provider": "p", "model": "m",
                                "hub_skills": [" a ", "b", "  "], "soul": "", "refusals": []}},
    {"provision_profile": {"path": "/h/w", "model_set": True, "model_error": None,
                           "mcp_written": None, "skills_disabled": [], "warnings": []},
     "spawn_install": [1, 2]})

# ══════════════ open_profile_terminal_endpoint ══════════════
add("open_terminal", {"name": "p", "platform": "win32"},
    {"resolve_profile_dir": "p setup", "popen": None})
add("open_terminal", {"name": "p", "platform": "darwin"},
    {"resolve_profile_dir": "p setup", "popen": None})
add("open_terminal", {"name": "default", "platform": "linux"},
    {"resolve_profile_dir": "hermes setup", "which": [1, 1, 1, 1, 1, 1, 1, 0, 1, 1], "popen": None})
add("open_terminal", {"name": "p", "platform": "linux"},
    {"resolve_profile_dir": "p setup", "which": [1, 1, 1, 1, 1, 1, 1, 1, 0, 1], "popen": None})
add("open_terminal", {"name": "p", "platform": "linux"},
    {"resolve_profile_dir": "p setup", "which": 1})
add("open_terminal", {"name": "p", "platform": "linux"},
    {"resolve_profile_dir": rs("RuntimeError", "resolve")})
add("open_terminal", {"name": "p", "platform": "linux"},
    {"resolve_profile_dir": "p setup", "which": [rs("OSError"), 0], "popen": None})
add("open_terminal", {"name": "p", "platform": "linux"},
    {"resolve_profile_dir": "p setup", "which": 1})

# ══════════════ _read_profiles / list_profiles_endpoint ══════════════
PROFS = [{"name": "default"}, {"name": "p"}]
for func in ["read_profiles", "list_profiles_endpoint"]:
    add(func, {}, {"list_profiles": {"__arr__": PROFS},
                   "scope_to_launch_profile": {"__arr__": PROFS}})
    add(func, {}, {"list_profiles": rs("OSError", "list"), "log_exception": None,
                   "fallback_profile_dicts": {"__arr__": PROFS},
                   "scope_to_launch_profile": {"__arr__": PROFS}})
    add(func, {}, {"list_profiles": rs("KeyboardInterrupt")})
    add(func, {}, {"list_profiles": {"__arr__": PROFS},
                   "scope_to_launch_profile": {"ok": True}})
    add(func, {}, {"list_profiles": {"__arr__": []},
                   "scope_to_launch_profile": {"__arr__": []}})
    add(func, {}, {"list_profiles": rs("ValueError", "bad"), "log_exception": None,
                   "fallback_profile_dicts": rs("RuntimeError", "fallback")})

# ══════════════ _read_desktop_overlay ══════════════
add("read_desktop_overlay", {"profile_dir": "/h/p"},
    {"read_text": '{"theme": "dark", "x": 1}'})
add("read_desktop_overlay", {"profile_dir": "/h/p"}, {"read_text": "null"})
add("read_desktop_overlay", {"profile_dir": "/h/p"},
    {"read_text": '{"valid": true, "n": 3}'})
add("read_desktop_overlay", {"profile_dir": "/h/p"}, {"read_text": rs("ValueError", "bad")})
add("read_desktop_overlay", {"profile_dir": "/h/p"}, {"read_text": rs("OSError", "read")})
add("read_desktop_overlay", {"profile_dir": "/h/p"},
    {"read_text": '{"nested": {"a": [1, 2]}}'})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
