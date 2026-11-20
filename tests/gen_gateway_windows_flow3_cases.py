#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_windows_flow3.jsonl — scripted-seam FLOW
cases for the gateway_windows_flow3 port (hermes_cli/gateway_windows.py
unclaimed remainder: console encodings/decode, service roots, launcher
settings/scripts, dead-launcher refresh/drift, stdio probes, deep probes).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides. List-valued seam results are
wrapped one level deeper ([[...]]); bare arrays are per-call queues.

Domains (documented in the port header):
- paths are POSIX strings; bytes are arrays of ints 0..255.
- code pages: cp437/cp1252/cp65001 implemented; other cpN -> LookupError.
- exit-diag data bytes are ASCII; launcher content strings are ASCII.
- no integral-float literals in str()-computed positions; no KeyError scripted
  raises; vbs_identity results are 2-element arrays; open results {"data":[..]}.
Run: python tests/gen_gateway_windows_flow3_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_windows_flow3.jsonl")
CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


K3 = {"GetConsoleOutputCP": True, "GetOEMCP": True, "GetACP": True}

# ── _windows_console_encodings ──
add("windows_console_encodings", {"kernel32": None}, {"schtasks_encoding": "utf-8"})
add("windows_console_encodings", {"kernel32": None}, {"schtasks_encoding": rs("RuntimeError", "boom")})
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": 65001, "get_oem_cp": 437, "get_acp": 1252})
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": 65001, "get_oem_cp": 65001, "get_acp": 437})
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": 0, "get_oem_cp": -1, "get_acp": 1252})
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": 0, "get_oem_cp": 0, "get_acp": 0, "schtasks_encoding": "cp1252"})
add("windows_console_encodings", {"kernel32": {"GetACP": True}}, {"get_acp": 1252})
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": rs("ValueError", "bad"), "get_oem_cp": rs("OSError", "io"),
     "get_acp": 437})
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": rs("RuntimeError", "boom")})
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": "65001", "get_oem_cp": "abc", "get_acp": True})
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": None})
add("windows_console_encodings", {"kernel32": {"GetACP": None}}, {})
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": 65001, "get_oem_cp": rs("SystemExit", "bye")})
add("windows_console_encodings", {"kernel32": {"GetACP": True}}, {})  # fail-closed get_acp
add("windows_console_encodings", {"kernel32": K3},
    {"get_console_output_cp": 5.9, "get_oem_cp": 437, "get_acp": 1252})

# ── _decode_schtasks_output ──
D_UTF8 = {"schtasks_encoding": "utf-8"}
add("decode_schtasks_output", {"data": [79, 75], "kernel32": None}, D_UTF8)
add("decode_schtasks_output", {"data": [0xC3, 0xA9], "kernel32": None}, D_UTF8)  # é
add("decode_schtasks_output", {"data": [], "kernel32": None}, D_UTF8)
add("decode_schtasks_output", {"data": [0xFF, 0xFE, 0x41, 0x00], "kernel32": None}, D_UTF8)
add("decode_schtasks_output", {"data": [0xFE, 0xFF, 0x00, 0x41], "kernel32": None}, D_UTF8)
add("decode_schtasks_output", {"data": [0xFF, 0xFE, 0x00, 0xD8, 0x00, 0xDC], "kernel32": None}, D_UTF8)
add("decode_schtasks_output", {"data": [0xFF, 0xFE], "kernel32": None}, D_UTF8)
add("decode_schtasks_output", {"data": [0xFF, 0xFE, 0x41], "kernel32": None},
    {"schtasks_encoding": "cp1252"})  # malformed utf-16 -> cp1252
add("decode_schtasks_output", {"data": [0xB0], "kernel32": {"GetConsoleOutputCP": True}},
    {"get_console_output_cp": 437})
add("decode_schtasks_output", {"data": [0x80], "kernel32": {"GetConsoleOutputCP": True}},
    {"get_console_output_cp": 1252})
add("decode_schtasks_output", {"data": [0x81], "kernel32": {"GetConsoleOutputCP": True}},
    {"get_console_output_cp": 1252})  # undefined -> replace fallback
add("decode_schtasks_output", {"data": [0x41], "kernel32": {"GetConsoleOutputCP": True}},
    {"get_console_output_cp": 99999, "schtasks_encoding": "utf-8"})  # LookupError -> utf-8 fallback? no: utf-8 first
add("decode_schtasks_output", {"data": [0xFF], "kernel32": {"GetConsoleOutputCP": True}},
    {"get_console_output_cp": 99999})  # LookupError -> replace
add("decode_schtasks_output", {"data": [0xE2, 0x82], "kernel32": None},
    {"schtasks_encoding": "cp99999"})  # truncated utf-8 -> replace
add("decode_schtasks_output", {"data": [0xE2, 0x82, 0xAC], "kernel32": None}, D_UTF8)  # €
add("decode_schtasks_output", {"data": [0x41], "kernel32": None},
    {"schtasks_encoding": rs("RuntimeError", "boom")})
add("decode_schtasks_output", {"data": [0xFF], "kernel32": {"GetOEMCP": True}},
    {"get_oem_cp": rs("KeyboardInterrupt", "stop")})
add("decode_schtasks_output", {"data": [0xFF], "kernel32": None}, {})  # fail-closed schtasks_encoding

# ── hermes_service_roots ──
ROOTS = {"project_root_str": "/proj", "hermes_home": "/h"}
add("hermes_service_roots", {"executable": "/venv/Scripts/python.exe"}, ROOTS)
add("hermes_service_roots", {"executable": None}, ROOTS)
add("hermes_service_roots", {"executable": "/venv/Scripts/python.exe"},
    {"project_root_str": None, "hermes_home": "/h"})
add("hermes_service_roots", {"executable": "/venv/Scripts/python.exe"},
    {"project_root_str": "/proj", "hermes_home": None})
add("hermes_service_roots", {"executable": "/venv/Scripts/python.exe"},
    {"project_root_str": rs("RuntimeError", "boom")})
add("hermes_service_roots", {"executable": "/venv/Scripts/python.exe"},
    {"project_root_str": "/proj", "hermes_home": rs("OSError", "io")})
add("hermes_service_roots", {"executable": "python.exe"},
    {"project_root_str": "/proj/./x/", "hermes_home": "/h"})
add("hermes_service_roots", {"executable": "/venv/Scripts/python.exe"}, {})  # fail-closed
add("hermes_service_roots", {"executable": "/venv/Scripts/python.exe"},
    {"project_root_str": "/proj", "hermes_home": rs("SystemExit", "bye")})
add("hermes_service_roots", {"executable": 5}, ROOTS)

# ── _launcher_settings ──
SETTINGS = {
    "import_project_root": "/proj",
    "import_profile_arg": None,
    "import_get_python_path": None,
    "hermes_home": "/h",
    "get_python_path": "/venv/Scripts/python.exe",
    "preserve_hermes_home_path": "/venv/Scripts/python.exe",
    "stable_gateway_working_dir": "/proj",
    "profile_arg": "--profile default",
}


def settings_seams(**over):
    s = dict(SETTINGS)
    s.update(over)
    return s


add("launcher_settings", {"home": None}, settings_seams())
add("launcher_settings", {"home": "/other"}, settings_seams())
add("launcher_settings", {"home": "/other/./x"}, settings_seams())
add("launcher_settings", {"home": None}, settings_seams(hermes_home=None))
add("launcher_settings", {"home": None}, settings_seams(get_python_path=rs("RuntimeError", "boom")))
add("launcher_settings", {"home": None}, settings_seams(preserve_hermes_home_path=rs("OSError", "io")))
add("launcher_settings", {"home": None}, settings_seams(stable_gateway_working_dir=rs("ValueError", "bad")))
add("launcher_settings", {"home": None}, settings_seams(profile_arg=rs("RuntimeError", "boom")))
add("launcher_settings", {"home": None}, settings_seams(import_project_root=rs("ImportError", "no module")))
add("launcher_settings", {"home": None}, settings_seams(import_profile_arg=rs("SystemExit", "bye")))
add("launcher_settings", {"home": None}, {})  # fail-closed import_project_root
add("launcher_settings", {"home": None}, settings_seams(get_python_path=rs("KeyboardInterrupt", "stop")))

# ── _launcher_scripts ──
TRIPLES = [["/h/gateway-service/Hermes_Gateway.cmd", "CMD\r\n", "utf-8"],
           ["/h/gateway-service/Hermes_Gateway.vbs", "VBS\r\n", "utf-16"]]
SCRIPTS_CHAIN = settings_seams(
    get_task_script_path="/h/gateway-service/Hermes_Gateway.cmd",
    render_launchers=[TRIPLES])
add("launcher_scripts", {}, SCRIPTS_CHAIN)
add("launcher_scripts", {}, settings_seams(profile_arg=rs("RuntimeError", "boom")))
add("launcher_scripts", {}, settings_seams(get_task_script_path=rs("OSError", "io")))
add("launcher_scripts", {}, settings_seams(render_launchers=rs("RuntimeError", "boom")))
add("launcher_scripts", {}, settings_seams(render_launchers=[TRIPLES],
                                           get_task_script_path=rs("SystemExit", "bye")))
add("launcher_scripts", {}, {})  # fail-closed
add("launcher_scripts", {}, settings_seams(get_task_script_path=None,
                                           render_launchers=[TRIPLES]))

# ── refresh_dead_launcher ──
VBS_ID = [["/h", ["/dead/python.exe", "-m", "hermes_cli.main"]]]


def refresh_seams(exists, resolve=None, normcase=None, read_bytes=None,
                  write=None, render=None, **over):
    s = {
        "import_vbs_identity": None,
        "import_launcher_text": None,
        "import_read_definition": None,
        "sanitize_filename": "Hermes_Gateway",
        "read_definition": "<Task/>",
        "launcher_text": "VBS TEXT",
        "vbs_identity": VBS_ID,
        "path_exists": exists,
        "path_resolve": resolve if resolve is not None else ["/h", "/h"],
        "normcase": normcase if normcase is not None else ["/h", "/h"],
        "write_launcher_scripts": write,
        "render_launchers": render if render is not None else [TRIPLES],
    }
    s.update(SETTINGS)
    if read_bytes is not None:
        s["path_read_bytes"] = read_bytes
    s.update(over)
    return s


add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], read_definition=rs("OSError", "io")))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], vbs_identity=rs("ValueError", "bad")))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], read_definition=rs("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)")))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], vbs_identity=[["/h", []]]))  # empty argv -> interpreter None
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], vbs_identity=[["/h", ["relative/python.exe"]]]))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([True]))  # interpreter alive
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([rs("OSError", "denied")]))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False, False, False], write=None))  # rewrite both
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], normcase=["/h", "/other"]))  # different home -> False
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False, True, True],
                  read_bytes=[[67, 77, 68, 13, 10], [255, 254, 86, 0, 66, 0, 83, 0, 13, 0, 10, 0]]))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False, True, True], read_bytes=[[0], [0]], write=None))  # both differ
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False, False, False], write=[rs("OSError", "denied"), None]))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False, rs("OSError", "denied"), False], write=None))
add("refresh_dead_launcher", {"home": None, "task_name": "Hermes_Gateway"},
    {"import_vbs_identity": None, "import_launcher_text": None, "import_read_definition": None})
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"}, {})  # fail-closed import
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    {"import_vbs_identity": rs("SystemExit", "bye")})
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], vbs_identity=[[None, ["/dead/python.exe"]]]))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], path_resolve=[rs("OSError", "io"), "/h"]))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], read_definition=rs("SystemExit", "bye")))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    {"import_vbs_identity": None, "import_launcher_text": None,
     "import_read_definition": None, "sanitize_filename": "Hermes_Gateway"})  # fail-closed read_definition
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], render=rs("RuntimeError", "boom")))
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False], vbs_identity=[["/h", ["/dead/python.exe"]], "extra"]))  # unpack too many
add("refresh_dead_launcher", {"home": "/h", "task_name": "Hermes_Gateway"},
    refresh_seams([False, True], read_bytes=[rs("OSError", "io")], write=None))

# ── launcher_script_drift ──
def drift_seams(read_bytes, **over):
    s = dict(SCRIPTS_CHAIN)
    s["assert_windows"] = None
    s["path_read_bytes"] = read_bytes
    s.update(over)
    return s


CMD_BYTES = [67, 77, 68, 13, 10]
VBS_BYTES = [255, 254, 86, 0, 66, 0, 83, 0, 13, 0, 10, 0]
add("launcher_script_drift", {}, drift_seams([CMD_BYTES, VBS_BYTES]))
add("launcher_script_drift", {}, drift_seams([[0], [0]]))
add("launcher_script_drift", {}, drift_seams([CMD_BYTES, [0]]))
add("launcher_script_drift", {}, drift_seams([rs("OSError", "io"), VBS_BYTES]))
add("launcher_script_drift", {}, drift_seams([CMD_BYTES, VBS_BYTES],
                                             assert_windows=rs("RuntimeError", "gateway_windows is Windows-only")))
add("launcher_script_drift", {}, drift_seams([CMD_BYTES, VBS_BYTES],
                                             assert_windows=rs("SystemExit", "bye")))
add("launcher_script_drift", {}, {"assert_windows": None})  # fail-closed settings chain
add("launcher_script_drift", {}, drift_seams([rs("KeyboardInterrupt", "stop")]))

# ── _stdout_isatty ──
SO = {"__stdout__": True}
add("stdout_isatty", {"stdout": None}, {})
add("stdout_isatty", {"stdout": SO}, {"stdout_isatty": True})
add("stdout_isatty", {"stdout": SO}, {"stdout_isatty": False})
add("stdout_isatty", {"stdout": 5}, {})
add("stdout_isatty", {"stdout": "x"}, {})
add("stdout_isatty", {"stdout": SO}, {"stdout_isatty": rs("RuntimeError", "boom")})
add("stdout_isatty", {"stdout": SO}, {"stdout_isatty": None})
add("stdout_isatty", {"stdout": SO}, {"stdout_isatty": rs("SystemExit", "bye")})
add("stdout_isatty", {"stdout": SO}, {})  # fail-closed
add("stdout_isatty", {"stdout": {}}, {})

# ── _stdin_console_mode_ok ──
add("stdin_console_mode_ok", {"platform": "linux", "kernel32": K3}, {})
add("stdin_console_mode_ok", {"platform": "win32", "kernel32": K3},
    {"get_std_handle": 123, "get_console_mode": 1})
add("stdin_console_mode_ok", {"platform": "win32", "kernel32": K3},
    {"get_std_handle": 123, "get_console_mode": 0})
add("stdin_console_mode_ok", {"platform": "win32", "kernel32": None}, {})
add("stdin_console_mode_ok", {"platform": "win32", "kernel32": K3},
    {"get_std_handle": rs("OSError", "io")})
add("stdin_console_mode_ok", {"platform": "win32", "kernel32": K3},
    {"get_std_handle": 123, "get_console_mode": None})
add("stdin_console_mode_ok", {"platform": "win32", "kernel32": K3}, {})  # fail-closed
add("stdin_console_mode_ok", {"platform": "win32", "kernel32": K3},
    {"get_std_handle": rs("GeneratorExit", "gone")})
add("stdin_console_mode_ok", {"platform": "darwin", "kernel32": None}, {})

# ── _probe ──
add("probe", {"index": 1, "ok": True, "message": "all good"}, {"print": None})
add("probe", {"index": 2, "ok": False, "message": "broken"}, {"print": None})
add("probe", {"index": 3, "ok": None, "message": "m"}, {"print": None})
add("probe", {"index": 4, "ok": "x", "message": "m"}, {"print": None})
add("probe", {"index": 5, "ok": 0, "message": "m"}, {"print": None})
add("probe", {"index": 6, "ok": 1, "message": "m"}, {"print": rs("RuntimeError", "boom")})
add("probe", {"index": 1, "ok": True, "message": "m"}, {})  # fail-closed print
add("probe", {"index": 1, "ok": True, "message": "m"}, {"print": rs("SystemExit", "bye")})

# ── _probe_missing ──
add("probe_missing", {"index": 1, "path": "/h/gateway.pid", "label": "PID file"},
    {"path_exists": True})
add("probe_missing", {"index": 1, "path": "/h/gateway.pid", "label": "PID file"},
    {"path_exists": False, "print": None})
add("probe_missing", {"index": 2, "path": "/h/./gateway.lock", "label": "Lock file"},
    {"path_exists": False, "print": None})
add("probe_missing", {"index": 1, "path": "/h/x", "label": "L"},
    {"path_exists": rs("OSError", "io")})
add("probe_missing", {"index": 1, "path": "/h/x", "label": "L"},
    {"path_exists": None, "print": None})
add("probe_missing", {"index": 1, "path": "/h/x", "label": "L"}, {})  # fail-closed
add("probe_missing", {"index": 1, "path": "/h/x", "label": "L"},
    {"path_exists": rs("KeyboardInterrupt", "stop")})
add("probe_missing", {"index": 1, "path": "/h/x", "label": "L"},
    {"path_exists": False, "print": rs("RuntimeError", "boom")})

# ── _probe_pid_file ──
def pid_seams(text, parsed, **over):
    s = {"path_exists": True, "path_read_text": text, "json_loads": parsed, "print": None}
    s.update(over)
    return s


add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    {"path_exists": False, "print": None})
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": 123}', {"pid": 123}))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": null}', {"pid": None}))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{}', {}))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{bad', rs("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)")))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams(None, None, path_read_text=rs("OSError", "io")))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('[1]', [1]))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": "abc"}', {"pid": "abc"}))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": []}', {"pid": []}))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": "5"}', {"pid": "5"}))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": "-5"}', {"pid": "-5"}))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": " +42 "}', {"pid": " +42 "}))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": 5.9}', {"pid": 5.9}))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams(None, None, path_read_text=rs("SystemExit", "bye")))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    {"path_exists": True, "print": None})  # fail-closed read_text -> caught
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": 1}', {"pid": 1}, print=[rs("RuntimeError", "boom"), None]))
add("probe_pid_file", {"pid_path": "/h/gateway.pid"},
    pid_seams('{"pid": true}', {"pid": True}))
add("probe_pid_file", {"pid_path": "/h/./gateway.pid"},
    pid_seams('{"pid": 7}', {"pid": 7}))

# ── _probe_lock_file ──
def lock_seams(held, **over):
    s = {"path_exists": True, "import_is_gateway_runtime_lock_active": None,
         "is_gateway_runtime_lock_active": held, "print": None}
    s.update(over)
    return s


add("probe_lock_file", {"lock_path": "/h/gateway.lock"},
    {"path_exists": False, "print": None})
add("probe_lock_file", {"lock_path": "/h/gateway.lock"}, lock_seams(True))
add("probe_lock_file", {"lock_path": "/h/gateway.lock"}, lock_seams(False))
add("probe_lock_file", {"lock_path": "/h/gateway.lock"},
    lock_seams(None, import_is_gateway_runtime_lock_active=rs("ImportError", "no module")))
add("probe_lock_file", {"lock_path": "/h/gateway.lock"},
    lock_seams(None, is_gateway_runtime_lock_active=rs("RuntimeError", "boom")))
add("probe_lock_file", {"lock_path": "/h/gateway.lock"},
    lock_seams(None, is_gateway_runtime_lock_active=rs("SystemExit", "bye")))
add("probe_lock_file", {"lock_path": "/h/gateway.lock"},
    {"path_exists": True, "print": None})  # fail-closed import -> caught
add("probe_lock_file", {"lock_path": "/h/gateway.lock"},
    lock_seams(True, print=[rs("RuntimeError", "boom"), None]))
add("probe_lock_file", {"lock_path": "/h/gateway.lock"}, lock_seams(None))

# ── _probe_running_pid ──
def runpid_seams(pid, **over):
    s = {"import_get_running_pid": None, "get_running_pid": pid, "print": None}
    s.update(over)
    return s


add("probe_running_pid", {}, runpid_seams(123))
add("probe_running_pid", {}, runpid_seams(None))
add("probe_running_pid", {}, runpid_seams(None, import_get_running_pid=rs("ImportError", "no module")))
add("probe_running_pid", {}, runpid_seams(None, get_running_pid=rs("RuntimeError", "boom")))
add("probe_running_pid", {}, runpid_seams(None, get_running_pid=rs("SystemExit", "bye")))
add("probe_running_pid", {}, {"print": None})  # fail-closed import -> caught repr
add("probe_running_pid", {}, runpid_seams(123, print=[rs("RuntimeError", "boom"), None]))
add("probe_running_pid", {}, runpid_seams(0))

# ── _probe_pid_exists ──
def pidex_seams(alive, **over):
    s = {"import_pid_exists": None, "pid_exists": alive, "print": None}
    s.update(over)
    return s


add("probe_pid_exists", {"candidate_pid": None}, {"print": None})
add("probe_pid_exists", {"candidate_pid": 123}, pidex_seams(True))
add("probe_pid_exists", {"candidate_pid": 123}, pidex_seams(False))
add("probe_pid_exists", {"candidate_pid": 123},
    pidex_seams(None, import_pid_exists=rs("ImportError", "no module")))
add("probe_pid_exists", {"candidate_pid": 123}, pidex_seams(None, pid_exists=rs("RuntimeError", "boom")))
add("probe_pid_exists", {"candidate_pid": 123}, pidex_seams(None, pid_exists=rs("KeyboardInterrupt", "stop")))
add("probe_pid_exists", {"candidate_pid": 123}, {"print": None})  # fail-closed import
add("probe_pid_exists", {"candidate_pid": 123}, pidex_seams(None))
add("probe_pid_exists", {"candidate_pid": 123}, pidex_seams(True, print=[rs("RuntimeError", "boom"), None]))

# ── _probe_state_file ──
NOW_EPOCH = 1795005296


def state_seams(parsed, dt=None, now=None, **over):
    s = {"path_exists": True, "path_read_text": "x", "json_loads": parsed, "print": None}
    if dt is not None:
        s["datetime_fromisoformat"] = dt
    if now is not None:
        s["datetime_now"] = now
    s.update(over)
    return s


add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    {"path_exists": False, "print": None})
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": "running"}))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": "degraded", "updated_at": "2026-11-18T12:34:50+00:00"},
                dt={"epoch": NOW_EPOCH - 6, "aware": True}, now=NOW_EPOCH))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": "stopped"}))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": "running", "updated_at": "2026-11-18T12:34:50"},
                dt={"epoch": NOW_EPOCH - 6, "aware": False}, now=NOW_EPOCH))  # naive -> inner TypeError
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": "running", "updated_at": "bad"},
                dt=rs("ValueError", "Invalid isoformat string: 'bad'"), now=NOW_EPOCH))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": "running", "updated_at": "2026-11-18T12:35:01.700000+00:00"},
                dt={"epoch": NOW_EPOCH + 5.7, "aware": True}, now=NOW_EPOCH))  # negative age trunc
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams(None, json_loads=rs("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)")))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams([1]))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams(None, path_read_text=rs("SystemExit", "bye")))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": "running", "updated_at": "x"},
                dt=rs("GeneratorExit", "gone"), now=NOW_EPOCH))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": "running", "updated_at": ""}))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    {"path_exists": True, "print": None})  # fail-closed read_text -> caught
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": None}))
add("probe_state_file", {"state_path": "/h/gateway_state.json"},
    state_seams({"gateway_state": "running", "updated_at": "x"},
                dt={"epoch": NOW_EPOCH - 6, "aware": True}, now=NOW_EPOCH,
                print=[rs("RuntimeError", "boom"), None]))

# ── _probe_exit_diag ──
def diag_seams(data, parsed=None, **over):
    s = {"path_exists": True, "open": {"data": data}, "print": None}
    if parsed is not None:
        s["json_loads"] = parsed
    s.update(over)
    return s


def lines_bytes(*lines):
    out = []
    for ln in lines:
        out.extend(ord(c) for c in ln)
        out.append(10)
    return out


EV = {"tag": "gateway.start", "pid": 123, "ts": "2026-11-18T12:00:00+00:00"}
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    {"path_exists": False, "print": None})
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams([]))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes("   ", "\t")))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes(json.dumps(EV)), EV))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes(json.dumps({"tag": "gateway.stop", "pid": 5, "ts": "t"})),
               {"tag": "gateway.stop", "pid": 5, "ts": "t"}))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes("not json"), rs("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)")))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes('"a string"'), "a string"))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams([], open=rs("OSError", "io")))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes("x" * 5000, json.dumps(EV)), EV))  # tail window
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes("y" * 200), rs("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)")))  # slice 120
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams([], open=rs("SystemExit", "bye")))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    {"path_exists": True, "print": None})  # fail-closed open -> caught
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes(json.dumps({"tag": "gateway.start"})), {"tag": "gateway.start"}))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes(json.dumps({"tag": "gateway.start", "pid": None, "ts": None})),
               {"tag": "gateway.start", "pid": None, "ts": None}))
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes("", json.dumps(EV), "", "  "), EV))  # reversed scan
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes("bad", json.dumps(EV)), EV))  # last non-empty wins
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(lines_bytes(json.dumps(EV)), EV,
               print=[rs("RuntimeError", "boom"), None]))  # inner catch -> second print
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    diag_seams(None))  # open {"data": null} -> bytes(None) TypeError -> outer catch
add("probe_exit_diag", {"diag_path": "/h/logs/gateway-exit-diag.log"},
    {"path_exists": True, "open": None, "print": None})  # open null -> None["data"] TypeError


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
        print("  %-28s %d" % (k, counts[k]))
    thin = [k for k, v in counts.items() if v < 5]
    if thin:
        raise SystemExit("FATAL: funcs with <5 cases: %s" % thin)


if __name__ == "__main__":
    main()