#!/usr/bin/env python3
"""Generate tests/fixtures/config_flow2b.jsonl for the config.py second-wave
UNCLAIMED FLOW oracle. Each line is {"func": <name>, "in": {...}}; "in.io"
carries scripted seam results ({"ret","raise","import_raise","state"} or a
queue LIST) consumed identically by the C harness dispatcher and the oracle's
recording stubs. List-valued seam results are wrapped in {"ret": [...]} so a
queue is never confused with a single list result.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "config_flow2b.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(v):
    return {"ret": v}


def X(t, msg="", **extra):
    d = {"type": t, "message": msg}
    d.update(extra)
    return {"raise": d}


def IMP(t, msg="", **extra):
    d = {"type": t, "message": msg}
    d.update(extra)
    return {"import_raise": d}


# Shared constants used by the offer/report flows.
OPT = {
    "A": {"description": "desc A", "prompt": "Enter A", "url": "https://a.example"},
    "B": {"description": ""},
    "C": {"description": "desc C", "prompt": "C key"},
}
BYVER = {"3": ["A", "B"], "4": ["C"]}


# ── 1. warn_invalid_platform_toolsets ────────────────────────────────────
BASE_W = {"read_raw_config": R({"platform_toolsets": {}}),
          "saved_toolset_resolver": R("res"), "validate_platform_toolsets": R([]),
          "logger.debug": R(None)}
case("warn_invalid_platform_toolsets", quiet=True, state={"results": {"warnings": []}}, io=BASE_W)
case("warn_invalid_platform_toolsets", quiet=False, state={"results": {"warnings": []}},
     io={"read_raw_config": R({"platform_toolsets": {"x": 1}}),
         "saved_toolset_resolver": R("res"),
         "validate_platform_toolsets": R(["bad one", "bad two"]), "logger.debug": R(None)})
case("warn_invalid_platform_toolsets", quiet=False, state={"results": {"warnings": []}},
     io={"read_raw_config": R({}), "saved_toolset_resolver": R("r"),
         "validate_platform_toolsets": X("ValueError", "validator broke"), "logger.debug": R(None)})
case("warn_invalid_platform_toolsets", quiet=True, state={"results": {"warnings": []}},
     io={"read_raw_config": X("OSError", "read fail"), "logger.debug": R(None)})
case("warn_invalid_platform_toolsets", quiet=True, state={"results": {"warnings": []}},
     io={"read_raw_config": R({}), "saved_toolset_resolver": X("TypeError", "resolver fail"),
         "logger.debug": R(None)})
case("warn_invalid_platform_toolsets", quiet=True, state={"results": {"warnings": []}},
     io={"read_raw_config": R({}), "saved_toolset_resolver": R("r"),
         "validate_platform_toolsets": X("KeyboardInterrupt", ""), "logger.debug": R(None)})


# ── 2. offer_new_optional_env_vars ───────────────────────────────────────
C2 = {"constants": {"env_vars_by_version": BYVER, "optional_env_vars": OPT},
      "state": {"results": {"env_added": [], "config_added": [], "warnings": []}}}
case("offer_new_optional_env_vars", current_ver=2, latest_ver=3,
     io={"get_env_value": [R(None), R(None)], "_offer_list": R(False)}, **C2)
case("offer_new_optional_env_vars", current_ver=2, latest_ver=3,
     io={"get_env_value": [R(None), R(None)], "_offer_list": R(True),
         "_prompt_and_save_env": R(None)}, **C2)
case("offer_new_optional_env_vars", current_ver=2, latest_ver=4,
     io={"get_env_value": [R(None), R("set"), R(None)], "_offer_list": R(True),
         "_prompt_and_save_env": [R(None), R(None)]}, **C2)
case("offer_new_optional_env_vars", current_ver=2, latest_ver=3,
     io={"get_env_value": [X("OSError", "env read"), R(None)]}, **C2)
case("offer_new_optional_env_vars", current_ver=2, latest_ver=3,
     io={"get_env_value": [R(None), R(None)], "_offer_list": X("ValueError", "declined")}, **C2)
case("offer_new_optional_env_vars", current_ver=5, latest_ver=3, io={}, **C2)
case("offer_new_optional_env_vars", current_ver=2, latest_ver=3,
     io={"get_env_value": [R(None), R(None)], "_offer_list": R(True),
         "_prompt_and_save_env": X("RuntimeError", "save fail")}, **C2)


# ── 3. offer_skill_config_vars ───────────────────────────────────────────
SK = [{"key": "a.token", "description": "A token", "prompt": "Token", "skill": "s1"},
      {"key": "b.url", "description": "B url", "prompt": "URL", "default": "http://x", "skill": "s2"}]
C3 = {"constants": {"skill_config_prefix": "skills.config"},
      "state": {"results": {"env_added": [], "config_added": [], "warnings": []}}}
case("offer_skill_config_vars", missing_skill_config=SK,
     io={"_offer_list": R(False)}, **C3)
case("offer_skill_config_vars", missing_skill_config=SK,
     io={"_offer_list": R(True), "read_raw_config": R({}),
         "line_input": [R(" entered "), R("")], "_set_nested": R(None),
         "_persist_migration": R(None)}, **C3)
case("offer_skill_config_vars", missing_skill_config=SK,
     io={"_offer_list": R(True), "read_raw_config": R({}),
         "line_input": [R(""), R("http://custom")], "_set_nested": R(None),
         "_persist_migration": R(None)}, **C3)
case("offer_skill_config_vars", missing_skill_config=[{"key": "k", "description": "D", "prompt": "P"}],
     io={"_offer_list": R(True), "read_raw_config": R({}), "line_input": R("v"),
         "_set_nested": R(None), "_persist_migration": R(None)}, **C3)
case("offer_skill_config_vars", missing_skill_config=SK,
     io={"_offer_list": R(True), "read_raw_config": R({}), "line_input": X("EOFError", "eof")}, **C3)
case("offer_skill_config_vars", missing_skill_config=SK,
     io={"_offer_list": R(True), "read_raw_config": X("OSError", "read")}, **C3)
case("offer_skill_config_vars", missing_skill_config=SK,
     io={"_offer_list": X("RuntimeError", "no")}, **C3)
case("offer_skill_config_vars", missing_skill_config=SK,
     constants={"skill_config_prefix_import_raise": True},
     state={"results": {"env_added": [], "config_added": [], "warnings": []}},
     io={"_offer_list": R(True), "read_raw_config": R({}), "line_input": [R("a"), R("b")],
         "_set_nested": R(None), "_persist_migration": R(None)})


# ── 4. note_policy ───────────────────────────────────────────────────────
for has, raw in [(True, "secret"), (True, 7), (True, None), (False, "x")]:
    case("note_policy", setter="register_note", raw=raw,
         redact={"has_setter": has}, io={"hermes_home_key": R("/h"), "note": R(None)})
case("note_policy", setter="other", raw="v", io={})
case("note_policy", setter="register_note", raw="v", redact={"has_setter": True},
     io={"hermes_home_key": X("ValueError", "no home"), "note": R(None)})
case("note_policy", setter="register_note", raw="v", redact={"has_setter": True},
     io={"hermes_home_key": R("/h"), "note": X("RuntimeError", "note fail")})


# ── 5. read_env_lines ────────────────────────────────────────────────────
case("read_env_lines", env_path="/h/.env",
     io={"open": R(["A=1\n", "B=2\n"]), "sanitize_env_lines": R(["A=1", "B=2"])})
case("read_env_lines", env_path="/h/.env",
     io={"open": R([]), "sanitize_env_lines": R([])})
case("read_env_lines", env_path="/h/.env", io={"open": X("OSError", "denied")})
case("read_env_lines", env_path="/h/.env",
     io={"open": R(["K=V\n"]), "sanitize_env_lines": X("ValueError", "bad")})
case("read_env_lines", env_path="", io={"open": R(["\n"]), "sanitize_env_lines": R([""])})


# ── 6. env_write_lock_holder ─────────────────────────────────────────────
for v in ["holder:/h/.env", None, {"local": 1}, 0, ""]:
    case("env_write_lock_holder", env_path="/h/.env", io={"lock_holder": R(v)})
case("env_write_lock_holder", env_path="/h/.env", io={"lock_holder": X("RuntimeError", "no")})


# ── 7. env_write_lock ────────────────────────────────────────────────────
case("env_write_lock", env_path="/h/.env", io={"lock_holder": R("h"), "file_lock": R(None)})
case("env_write_lock", env_path="/h/sub/.env", io={"lock_holder": R("h"), "file_lock": R("cm")})
case("env_write_lock", env_path="/h/.env", io={"lock_holder": R("h"), "file_lock": X("OSError", "timeout")})
case("env_write_lock", env_path="/h/.env", io={"lock_holder": X("ValueError", "holder"), "file_lock": R(None)})
case("env_write_lock", env_path="/h/.env",
     io={"lock_holder": R("h"), "file_lock": X("KeyboardInterrupt", "")})


# ── 8. write_env_lines ───────────────────────────────────────────────────
def wio(**over):
    d = {"mkstemp_beside": R([3, "/h/.env_abc.tmp"]), "fdopen": R("f"),
         "writelines": R(None), "flush": R(None), "fsync": R(None),
         "atomic_replace": R(None), "secure_file": R(None), "unlink": R(None),
         "chmod": R(None)}
    d.update(over)
    return d
case("write_env_lines", env_path="/h/.env", lines=["A=1\n"],
     io=wio())
case("write_env_lines", env_path="/h/.env", lines=["A=1\n"], preserve_mode=True,
     io=wio(path_stat=R({"mode": 420})))
case("write_env_lines", env_path="/h/.env", lines=[], preserve_mode=True,
     io=wio(path_stat=X("OSError", "stat fail")))
case("write_env_lines", env_path="/h/.env", lines=["A=1\n"],
     io=wio(writelines=X("ValueError", "write fail")))
case("write_env_lines", env_path="/h/.env", lines=["A=1\n"],
     io=wio(atomic_replace=X("RuntimeError", "replace fail")))
case("write_env_lines", env_path="/h/.env", lines=["A=1\n"],
     io=wio(atomic_replace=X("RuntimeError", "replace fail"), unlink=X("OSError", "unlink fail")))
case("write_env_lines", env_path="/h/.env", lines=["A=1\n"], preserve_mode=True,
     io=wio(path_stat=R({"mode": 420}), chmod=X("OSError", "chmod fail")))
case("write_env_lines", env_path="/h/.env", lines=["A=1\n"],
     io=wio(secure_file=X("OSError", "secure_file failed")))
case("write_env_lines", env_path="/h/.env", lines=["A=1\n"],
     io={"mkstemp_beside": X("OSError", "no tmp")})


# ── 9. cmd_config_migrate ────────────────────────────────────────────────
MIG = {"env_added": [], "config_added": [], "warnings": [], "failed_step": None}
case("cmd_config_migrate", io={"print_banner": R(None), "get_missing_env_vars": R([]),
     "get_missing_config_fields": R([]), "check_config_version": R([3, 3])})
case("cmd_config_migrate", io={"print_banner": R(None), "get_missing_env_vars": R([]),
     "get_missing_config_fields": R(["x"]), "check_config_version": R([2, 3]),
     "migrate_config": R(MIG)})
case("cmd_config_migrate", io={"print_banner": R(None),
     "get_missing_env_vars": R([{"name": "K", "is_required": True}]),
     "get_missing_config_fields": R([]), "check_config_version": R([3, 3]),
     "migrate_config": R({"env_added": ["K"], "config_added": [], "warnings": [], "failed_step": None})})
case("cmd_config_migrate", io={"print_banner": R(None),
     "get_missing_env_vars": R([{"name": "O", "is_required": False, "advanced": False,
                                 "tools": ["a", "b"]}]),
     "get_missing_config_fields": R([]), "check_config_version": R([3, 3]),
     "tools_suffix": R(" (enables: a, b)"), "migrate_config": R(MIG)})
case("cmd_config_migrate", io={"print_banner": R(None), "get_missing_env_vars": R([]),
     "get_missing_config_fields": R([]), "check_config_version": R([3, 3]),
     "migrate_config": R({"env_added": [], "config_added": [], "warnings": ["w1", "w2"],
                          "failed_step": None})})
case("cmd_config_migrate", io={"print_banner": R(None), "get_missing_env_vars": R([]),
     "get_missing_config_fields": R([]), "check_config_version": R([3, 3]),
     "migrate_config": R({"env_added": [], "config_added": [], "warnings": [],
                          "failed_step": "boom"})})
case("cmd_config_migrate", io={"print_banner": R(None), "get_missing_env_vars": R([]),
     "get_missing_config_fields": R([]), "check_config_version": X("ValueError", "parse")})
case("cmd_config_migrate", io={"print_banner": R(None), "get_missing_env_vars": R([]),
     "get_missing_config_fields": R([]), "check_config_version": R([3, 3]),
     "migrate_config": X("RuntimeError", "migrate fail")})


# ── 10. cmd_config_check ─────────────────────────────────────────────────
CHK = {"constants": {"required_env_vars": {}, "optional_env_vars": {"A": {"tools": ["x"]}, "B": {}}}}
case("cmd_config_check",
     io={"print_banner": R(None), "check_config_version": R([2, 2]),
         "get_env_value": [R("v"), R(None)], "tools_suffix": R(" → x"),
         "get_missing_config_fields": R([]), "read_raw_config_readonly": R({}),
         "config_check_diagnostics": R([])}, **CHK)
case("cmd_config_check",
     io={"print_banner": R(None), "check_config_version": R([1, 3]),
         "get_env_value": [R(None), R("v")], "get_missing_config_fields": R(["x"]),
         "read_raw_config_readonly": R({}),
         "config_check_diagnostics": R(["d1", "d2"])}, **CHK)
case("cmd_config_check", constants={"required_env_vars": {"K": {}}, "optional_env_vars": {}},
     io={"print_banner": R(None), "check_config_version": R([2, 2]),
         "get_env_value": R(None), "get_missing_config_fields": R([]),
         "read_raw_config_readonly": R({}), "config_check_diagnostics": R([])})
case("cmd_config_check",
     io={"print_banner": R(None), "check_config_version": X("ValueError", "bad")}, **CHK)
case("cmd_config_check",
     io={"print_banner": R(None), "check_config_version": R([2, 2]),
         "get_env_value": X("OSError", "env")}, **CHK)
case("cmd_config_check",
     io={"print_banner": R(None), "check_config_version": R([2, 2]),
         "get_env_value": [R("v"), R("v")], "get_missing_config_fields": R([]),
         "read_raw_config_readonly": R({}),
         "config_check_diagnostics": X("RuntimeError", "diag")}, **CHK)


# ── 11. platform_manifest_paths ──────────────────────────────────────────
case("platform_manifest_paths", source="bundled", io={"get_project_root": R("/proj"), "os.scandir": R([])})
case("platform_manifest_paths", source="bundled", io={"get_project_root": R("/p"), "os.scandir": X("FileNotFoundError", "gone")})
case("platform_manifest_paths", source="bundled", io={"get_project_root": R("/p"), "os.scandir": X("OSError", "denied")})
case("platform_manifest_paths", source="user", home="/h",
     io={"os.scandir": [
         R([{"name": "p1", "path": "/h/plugins/platforms/p1", "is_dir": True}]),
         R([])],
         "path_stat": R({"mode": 33188})})
case("platform_manifest_paths", source="user", home="/h",
     io={"os.scandir": [
         R([{"name": "__pycache__", "path": "/h/plugins/platforms/__pycache__", "is_dir": True}]),
         R([])]})
case("platform_manifest_paths", source="user", home="/h",
     io={"os.scandir": [
         R([{"name": "p2", "path": "/h/plugins/platforms/p2", "is_dir": True}]),
         R([])], "path_stat": X("PermissionError", "no search")})
case("platform_manifest_paths", source="user", home="/h",
     io={"os.scandir": [
         R([{"name": "p3", "path": "/h/plugins/platforms/p3", "is_dir": True}]),
         R([])], "path_stat": R({"mode": 16877})})
case("platform_manifest_paths", source="user", home="/h",
     io={"os.scandir": [
         R([{"name": "p4", "path": "/h/plugins/platforms/p4", "is_dir": True}]),
         R([])], "path_stat": [X("FileNotFoundError", "no yaml"), R({"mode": 33188})]})


# ── 12. platform_manifest_stamp ──────────────────────────────────────────
case("manifest_stamp", home="/h",
     io={"os.scandir": [R([{"name": "p1", "path": "/h/plugins/platforms/p1", "is_dir": True}]), R([])],
         "path_stat": R({"mode": 33188}), "file_signature": R("sig1")})
case("manifest_stamp", home="/h",
     io={"os.scandir": [R([]), R([{"name": "flat", "path": "/h/plugins/flat", "is_dir": True}])],
         "path_stat": R({"mode": 33188}), "file_signature": R("sig2")})
case("manifest_stamp", home="/h",
     io={"os.scandir": [R([{"name": "p", "path": "/h/plugins/platforms/p", "is_dir": True}]), R([])],
         "path_stat": X("PermissionError", "denied")})
case("manifest_stamp", home="/h",
     io={"os.scandir": [R([]), R([])]})
case("manifest_stamp", home="/h",
     io={"os.scandir": [R([{"name": "p", "path": "/h/plugins/platforms/p", "is_dir": True}]), R([])],
         "path_stat": R({"mode": 33188}), "file_signature": X("OSError", "sig")})


# ── 13. platform_plugin_manifests ────────────────────────────────────────
def ppaths(entries1, entries2, stat=None):
    d = {"os.scandir": [R(entries1), R(entries2)]}
    if stat is not None:
        d["path_stat"] = stat
    return d
case("platform_plugin_manifests", home="/h", source="user",
     state={"skipped": []}, io={"safe_worker_enabled": R(True)})
case("platform_plugin_manifests", home="/h", source="bundled",
     state={"skipped": []},
     io={"safe_worker_enabled": R(False), "get_project_root": R("/p"), "os.scandir": R([])})
case("platform_plugin_manifests", home="/h", source="user",
     state={"skipped": []},
     io={"safe_worker_enabled": R(False), "os.scandir": [X("OSError", "denied"), R([])],
         **{"logger.warning": R(None)}})
case("platform_plugin_manifests", home="/h", source="user",
     state={"skipped": []},
     io=dict(ppaths([{"name": "p", "path": "/h/plugins/platforms/p", "is_dir": True}], [],
                    R({"mode": 33188})),
             safe_worker_enabled=R(False), open=R("kind: platform\nname: p\n"),
             fast_safe_load=R({"kind": "platform", "name": "p"})))
case("platform_plugin_manifests", home="/h", source="user",
     state={"skipped": []},
     io=dict(ppaths([], [{"name": "flat", "path": "/h/plugins/flat", "is_dir": True}],
                    R({"mode": 33188})),
             safe_worker_enabled=R(False), open=R("kind: other\n"),
             fast_safe_load=R({"kind": "other"})))
case("platform_plugin_manifests", home="/h", source="user",
     state={"skipped": []},
     io=dict(ppaths([{"name": "p", "path": "/h/plugins/platforms/p", "is_dir": True}], [],
                    R({"mode": 33188})),
             safe_worker_enabled=R(False), open=X("OSError", "unreadable"),
             **{"logger.warning": R(None)}))
case("platform_plugin_manifests", home="/h", source="user", strict=True,
     state={"skipped": []},
     io=dict(ppaths([{"name": "p", "path": "/h/plugins/platforms/p", "is_dir": True}], [],
                    R({"mode": 33188})),
             safe_worker_enabled=R(False), open=X("OSError", "unreadable")))
case("platform_plugin_manifests", home="/h", source="user",
     state={"skipped": []},
     io=dict(ppaths([{"name": "p", "path": "/h/plugins/platforms/p", "is_dir": True}], [],
                    R({"mode": 33188})),
             safe_worker_enabled=R(False), open=R("bad:"),
             fast_safe_load=X("ValueError", "bad yaml")))
case("platform_plugin_manifests", home="/h", source="user",
     state={"skipped": []},
     io=dict(ppaths([{"name": "p", "path": "/h/plugins/platforms/p", "is_dir": True}], [],
                    R({"mode": 33188})),
             safe_worker_enabled=R(False), open=R(""), fast_safe_load=R(None)))


with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT}")
