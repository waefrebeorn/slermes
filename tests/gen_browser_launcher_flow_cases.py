#!/usr/bin/env python3
"""Generate tests/fixtures/browser_launcher_flow.jsonl — scripted-seam cases
for the browser_launcher_flow decision-sequence oracle.

Each line: {"func": <live python name>, "in": {...}}. "in.io" maps seam names
to scripts (absent/null -> None; {"seq": [...]} -> per-call sequence, None
when exhausted; any other value -> reused). Raise-specs are
{"__raise__": <ExceptionClass>, "msg": <str(exc)>}. read_bytes scripts carry
{"b64": ...}. Paths are POSIX-spelled; the oracle normalizes Path results
with as_posix(). >=5 cases per ported function.

Run: python tests/gen_browser_launcher_flow_cases.py
"""
import base64
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "browser_launcher_flow.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def b64(data):
    return {"b64": base64.b64encode(data).decode("ascii")}


def raise_spec(cls, msg):
    return {"__raise__": cls, "msg": msg}


# ── running_in_docker ──
case("running_in_docker", io={"exists": True})
case("running_in_docker", io={"exists": False, "open": "12:devices:/docker/abc\n"})
case("running_in_docker", io={"exists": False, "open": "0::/init.scope\n"})
case("running_in_docker", io={"exists": False, "open": raise_spec("OSError", "no cgroup")})
case("running_in_docker", io={"exists": False, "open": "xxnotdockerxx"})
case("running_in_docker", io={"exists": False, "open": ""})
case("running_in_docker", io={"exists": False, "open": "doc\rker"})

# ── apparmor_restricts_unprivileged_userns ──
for content, in [("1",), ("1\n",), (" 1 ",), ("0",), ("",), ("11",)]:
    case("apparmor_restricts_unprivileged_userns", io={"open": content})
case("apparmor_restricts_unprivileged_userns",
     io={"open": raise_spec("OSError", "no apparmor file")})

# ── needs_chromium_sandbox_bypass ──
case("needs_chromium_sandbox_bypass", io={"geteuid": 0})
case("needs_chromium_sandbox_bypass",
     io={"geteuid": 1000, "running_in_docker": True})
case("needs_chromium_sandbox_bypass",
     io={"geteuid": 1000, "running_in_docker": False,
         "apparmor_restricts_unprivileged_userns": True})
case("needs_chromium_sandbox_bypass",
     io={"geteuid": 1000, "running_in_docker": False,
         "apparmor_restricts_unprivileged_userns": False})
case("needs_chromium_sandbox_bypass", has_geteuid=False,
     io={"running_in_docker": False,
         "apparmor_restricts_unprivileged_userns": True})
case("needs_chromium_sandbox_bypass", has_geteuid=False,
     io={"running_in_docker": True})
case("needs_chromium_sandbox_bypass",
     io={"geteuid": raise_spec("OSError", "no euid")})

# ── _explicit_executable_configured ──
case("_explicit_executable_configured",
     io={"chromium_executable": {"seq": ["/bin/chrome", "/bin/chrome"]}})
case("_explicit_executable_configured",
     io={"chromium_executable": {"seq": ["/a", "/b"]}})
case("_explicit_executable_configured",
     io={"chromium_executable": {"seq": [None, None]}})
case("_explicit_executable_configured",
     io={"chromium_executable": {"seq": [None, "/b"]}})
case("_explicit_executable_configured",
     io={"chromium_executable": {"seq": ["", ""]}})
case("_explicit_executable_configured",
     io={"chromium_executable": {"seq": ["", None]}})
case("_explicit_executable_configured",
     io={"chromium_executable": {"seq": [raise_spec("RuntimeError", "cfg"), "/b"]}})

# ── _resolve_executable ──
case("_resolve_executable", io={"chromium_executable": None})
case("_resolve_executable", io={"chromium_executable": ""})
case("_resolve_executable",
     io={"chromium_executable": "/bin/chrome", "isfile": True})
case("_resolve_executable",
     io={"chromium_executable": "chrome", "isfile": False,
         "locate_command": {"command": ["/usr/bin/chrome", "/x"]}})
case("_resolve_executable",
     io={"chromium_executable": "chrome", "isfile": False,
         "locate_command": {"command": []}})
case("_resolve_executable",
     io={"chromium_executable": "chrome", "isfile": False,
         "locate_command": {"command": None}})
case("_resolve_executable",
     io={"chromium_executable": raise_spec("OSError", "x")})
case("_resolve_executable",
     io={"chromium_executable": "chrome", "isfile": False,
         "locate_command": raise_spec("RuntimeError", "resolver down")})

# ── chromium_ready ──
case("chromium_ready", io={"_resolve_executable": "/bin/chrome"})
case("chromium_ready",
     io={"_resolve_executable": None, "_maybe_autoinstall_chromium": False,
         "_chromium_missing_hint": "install chromium"})
case("chromium_ready",
     io={"_resolve_executable": {"seq": [None, "/bin/chrome"]},
         "_maybe_autoinstall_chromium": True})
case("chromium_ready",
     io={"_resolve_executable": {"seq": [None, None]},
         "_maybe_autoinstall_chromium": True,
         "_chromium_missing_hint": "still missing"})
case("chromium_ready",
     io={"_resolve_executable": {"seq": ["", "/bin/chrome"]},
         "_maybe_autoinstall_chromium": True})
case("chromium_ready",
     io={"_resolve_executable": None, "_maybe_autoinstall_chromium": False,
         "_chromium_missing_hint": raise_spec("RuntimeError", "no hint")})
case("chromium_ready",
     io={"_resolve_executable": None,
         "_maybe_autoinstall_chromium": raise_spec("OSError", "pm down")})

# ── _launch_timeout ──
case("_launch_timeout", timeout=30)
case("_launch_timeout", timeout=0)
case("_launch_timeout", timeout=0.5)
case("_launch_timeout", timeout=None, io={"command_timeout": 300})
case("_launch_timeout", timeout=None, io={"command_timeout": 60})
case("_launch_timeout", timeout=None, io={"command_timeout": 120})
case("_launch_timeout", timeout=None,
     io={"command_timeout": raise_spec("RuntimeError", "no config")})

# ── _resolve_headed ──
case("_resolve_headed", headed=None, io={"is_headed_mode": False})
case("_resolve_headed", headed=None, env={"DISPLAY": ":0"},
     io={"is_headed_mode": True, "_has_display": True})
case("_resolve_headed", headed=None, env={}, platform="linux",
     io={"is_headed_mode": True, "_has_display": False})
case("_resolve_headed", headed=None, env={}, platform="win32",
     io={"is_headed_mode": True, "_has_display": False})
case("_resolve_headed", headed=False, env={"DISPLAY": ":0"})
case("_resolve_headed", headed=True, env={}, platform="darwin",
     io={"_has_display": False})
case("_resolve_headed", headed=None,
     io={"is_headed_mode": raise_spec("RuntimeError", "x")})
case("_resolve_headed", headed=True, env={"DISPLAY": ":0"}, platform="linux",
     io={"_has_display": True})

# ── _spawn_kwargs ──
case("_spawn_kwargs", os_name="posix")
case("_spawn_kwargs", os_name="nt", create_new_process_group=512,
     io={"windows_hide_flags": 0x08000000})
case("_spawn_kwargs", os_name="nt", create_new_process_group=None,
     io={"windows_hide_flags": 0})
case("_spawn_kwargs", os_name="nt", create_new_process_group=512,
     io={"windows_hide_flags": 8})
case("_spawn_kwargs", os_name="nt", create_new_process_group=0,
     io={"windows_hide_flags": 8})
case("_spawn_kwargs", os_name="nt",
     io={"windows_hide_flags": raise_spec("OSError", "x")})

# ── _read_devtools_file ──
UDD = "/h/cache/browser/chromium/d-1"
case("_read_devtools_file", user_data_dir=UDD,
     io={"open": "1234\n/devtools/browser/abc\n"})
case("_read_devtools_file", user_data_dir=UDD,
     io={"open": "\ufeff1234\n/devtools/browser/abc\n"})
case("_read_devtools_file", user_data_dir=UDD,
     io={"open": "1234\r\n/devtools/browser/abc\r\n"})
case("_read_devtools_file", user_data_dir=UDD,
     io={"open": "abc\n/devtools/browser/abc\n"})
case("_read_devtools_file", user_data_dir=UDD,
     io={"open": "1234\n/devtools/page/abc\n"})
case("_read_devtools_file", user_data_dir=UDD, io={"open": "1234\n"})
case("_read_devtools_file", user_data_dir=UDD, io={"open": "1234"})
case("_read_devtools_file", user_data_dir=UDD, io={"open": ""})
case("_read_devtools_file", user_data_dir=UDD,
     io={"open": raise_spec("FileNotFoundError", "no file")})
case("_read_devtools_file", user_data_dir=UDD,
     io={"open": "0\n/devtools/browser/x\n"})
case("_read_devtools_file", user_data_dir=UDD,
     io={"open": "  1234  \n  /devtools/browser/abc  \n"})

# ── _log_tail ──
case("_log_tail", user_data_dir=UDD, io={"read_bytes": b64(b"hello log")})
case("_log_tail", user_data_dir=UDD, io={"read_bytes": b64(b"  hello \n")})
case("_log_tail", user_data_dir=UDD,
     io={"read_bytes": b64(b"A" * 2000 + b"TAIL")})
case("_log_tail", user_data_dir=UDD,
     io={"read_bytes": b64("h\u00e9llo \u2192".encode("utf-8"))})
case("_log_tail", user_data_dir=UDD,
     io={"read_bytes": b64(b"ok\xff\xfeend")})
case("_log_tail", user_data_dir=UDD,
     io={"read_bytes": b64(b"ok\xe2\x82")})
case("_log_tail", user_data_dir=UDD,
     io={"read_bytes": raise_spec("OSError", "no log")})
case("_log_tail", user_data_dir=UDD, io={"read_bytes": b64(b"")})

# ── _wait_for_devtools ──
FOUND = [1234, "/devtools/browser/x"]
case("_wait_for_devtools", user_data_dir=UDD, deadline=100,
     io={"_read_devtools_file": FOUND})
case("_wait_for_devtools", user_data_dir=UDD, deadline=100,
     io={"_read_devtools_file": None, "proc.poll": 0,
         "_log_tail": "Failed to start SANDBOX helper"})
case("_wait_for_devtools", user_data_dir=UDD, deadline=100,
     io={"_read_devtools_file": None, "proc.poll": 1,
         "_log_tail": "some failure"})
case("_wait_for_devtools", user_data_dir=UDD, deadline=100,
     io={"_read_devtools_file": None, "proc.poll": 2, "_log_tail": ""})
case("_wait_for_devtools", user_data_dir=UDD, deadline=100,
     io={"_read_devtools_file": None, "proc.poll": None, "monotonic": 100.5})
case("_wait_for_devtools", user_data_dir=UDD, deadline=100,
     io={"_read_devtools_file": {"seq": [None, None, FOUND]},
         "proc.poll": {"seq": [None, None]},
         "monotonic": {"seq": [0.05, 0.15]}})
case("_wait_for_devtools", user_data_dir=UDD, deadline=100,
     io={"_read_devtools_file": None, "proc.poll": -9, "_log_tail": ""})
case("_wait_for_devtools", user_data_dir=UDD, deadline=100,
     io={"_read_devtools_file": raise_spec("OSError", "x")})

# ── _singleton_owner ──
case("_singleton_owner", user_data_dir=UDD, io={"readlink": "myhost-1234"})
case("_singleton_owner", user_data_dir=UDD, io={"readlink": "1234"})
case("_singleton_owner", user_data_dir=UDD, io={"readlink": "my-host-name-456"})
case("_singleton_owner", user_data_dir=UDD, io={"readlink": "myhost-abc"})
case("_singleton_owner", user_data_dir=UDD, io={"readlink": "myhost-"})
case("_singleton_owner", user_data_dir=UDD, io={"readlink": "-7"})
case("_singleton_owner", user_data_dir=UDD,
     io={"readlink": raise_spec("OSError", "no lock")})

# ── _singleton_pid ──
case("_singleton_pid", user_data_dir=UDD, io={"_singleton_owner": ["h", 1234]})
case("_singleton_pid", user_data_dir=UDD, os_name="nt",
     io={"_singleton_owner": None, "_pid_naming_dir": 777})
case("_singleton_pid", user_data_dir=UDD, os_name="posix",
     io={"_singleton_owner": None})
case("_singleton_pid", user_data_dir=UDD, os_name="nt",
     io={"_singleton_owner": None, "_pid_naming_dir": None})
case("_singleton_pid", user_data_dir=UDD, io={"_singleton_owner": ["", 0]})
case("_singleton_pid", user_data_dir=UDD, os_name="nt",
     io={"_singleton_owner": None,
         "_pid_naming_dir": raise_spec("RuntimeError", "psutil")})

# ── running_instance ──
case("running_instance", user_data_dir=UDD,
     io={"_singleton_pid": 1234, "_read_devtools_file": [9222, "/devtools/browser/x"],
         "_pid_alive": True, "_devtools_endpoint_live": True})
case("running_instance", user_data_dir=UDD,
     io={"_singleton_pid": None, "_read_devtools_file": [9222, "/devtools/browser/x"]})
case("running_instance", user_data_dir=UDD,
     io={"_singleton_pid": 1234, "_read_devtools_file": None})
case("running_instance", user_data_dir=UDD,
     io={"_singleton_pid": 1234, "_read_devtools_file": [9222, "/devtools/browser/x"],
         "_pid_alive": False})
case("running_instance", user_data_dir=UDD,
     io={"_singleton_pid": 1234, "_read_devtools_file": [9222, "/devtools/browser/x"],
         "_pid_alive": True, "_devtools_endpoint_live": False})
case("running_instance", user_data_dir=UDD,
     io={"_singleton_pid": raise_spec("RuntimeError", "x")})

# ── _default_user_data_dir ──
case("_default_user_data_dir", key="browser-exec-default", home_key="/h/home",
     io={"getpid": 4242, "_start_time": 1700000000, "ns_tag": ""})
case("_default_user_data_dir", key="browser-exec-default", home_key="/h/home",
     io={"getpid": 4242, "_start_time": None, "ns_tag": ""})
case("_default_user_data_dir", key="browser-exec-default", home_key="/h/home",
     io={"getpid": 4242, "_start_time": 1700000000, "ns_tag": "-n0123456789ab"})
case("_default_user_data_dir", key="session-a", home_key="/h/home",
     io={"getpid": 4242, "_start_time": 1700000000, "ns_tag": ""})
case("_default_user_data_dir", key="browser-exec-default", home_key="",
     io={"getpid": 4242, "_start_time": None, "ns_tag": ""})
case("_default_user_data_dir", key="k", home_key="/h/home",
     io={"getpid": raise_spec("OSError", "x")})
case("_default_user_data_dir", key="k", home_key="/h/home",
     io={"getpid": 4242, "_start_time": raise_spec("OSError", "x")})

# ── _prepare_dir ──
PUDD = "/h/cache/browser/chromium/d-1"
case("_prepare_dir", user_data_dir=PUDD, ephemeral=True,
     io={"mkdir": None, "rmtree": None, "makedirs": None, "unlink": None})
case("_prepare_dir", user_data_dir="/profiles/p1", ephemeral=False,
     io={"makedirs": None, "unlink": None})
case("_prepare_dir", user_data_dir=PUDD, ephemeral=True,
     io={"mkdir": None, "rmtree": None, "makedirs": None,
         "unlink": raise_spec("FileNotFoundError", "gone")})
case("_prepare_dir", user_data_dir=PUDD, ephemeral=True,
     io={"mkdir": None, "rmtree": None,
         "makedirs": raise_spec("PermissionError", "denied")})
case("_prepare_dir", user_data_dir=PUDD, ephemeral=True,
     io={"mkdir": raise_spec("OSError", "denied")})
case("_prepare_dir", user_data_dir=PUDD, ephemeral=True,
     io={"mkdir": None, "rmtree": raise_spec("OSError", "busy"),
         "makedirs": None, "unlink": None})
case("_prepare_dir", user_data_dir=PUDD, ephemeral=False,
     io={"makedirs": None, "unlink": raise_spec("ValueError", "bad")})

# ── launch_chromium ──
BROWSER = {"key": "k1", "pid": 10, "owned": True}
case("launch_chromium", key="k1", io={"_key_lock": None, "get_browser": BROWSER})
case("launch_chromium", key="k2",
     io={"_key_lock": None, "get_browser": None, "hermes_home_key": "/h/home",
         "chromium_ready": [False, "install chromium first"]})
case("launch_chromium", key="k3", executable=None, ephemeral=True,
     io={"_key_lock": None, "get_browser": None, "hermes_home_key": "/h/home",
         "chromium_ready": [True, ""], "_resolve_executable": "/bin/chrome",
         "_default_user_data_dir": "/h/home/cache/browser/chromium/d-1",
         "_launch_owned": [{"key": "k3", "pid": 5}, None]})
case("launch_chromium", key="k4", executable="/usr/bin/google-chrome",
     ephemeral=False, user_data_dir="/profiles/p1",
     io={"_key_lock": None, "get_browser": None, "hermes_home_key": "/h/home",
         "_start_shared": [None, "refused"]})
case("launch_chromium", key="k5", executable="/x", ephemeral=False,
     user_data_dir=None,
     io={"_key_lock": None, "get_browser": None, "hermes_home_key": "/h/home",
         "_default_user_data_dir": "/h/home/cache/browser/chromium/d-2",
         "_start_shared": [{"key": "k5", "pid": 9}, None]})
case("launch_chromium", key="k6", executable="/x", ephemeral=False,
     user_data_dir="/p", prepare="fn",
     io={"_key_lock": None, "get_browser": None, "hermes_home_key": "/h/home",
         "_start_shared": [None, "in use"]})
case("launch_chromium", key="k7", executable="/x", ephemeral=False,
     user_data_dir="",
     io={"_key_lock": None, "get_browser": None, "hermes_home_key": "/h/home",
         "_default_user_data_dir": "/h/home/cache/browser/chromium/d-3",
         "_start_shared": [None, "ok"]})
case("launch_chromium", key="k8",
     io={"_key_lock": None, "get_browser": raise_spec("RuntimeError", "x")})
case("launch_chromium", key="k9", executable=None, ephemeral=True,
     io={"_key_lock": None, "get_browser": None, "hermes_home_key": "/h/home",
         "chromium_ready": [True, ""], "_resolve_executable": "/bin/chrome",
         "_default_user_data_dir": "/h/home/cache/browser/chromium/d-4",
         "_launch_owned": raise_spec("OSError", "spawn")})
case("launch_chromium", key="k10", executable="/usr/bin/chrome", ephemeral=True,
     headed=True, env={"DISPLAY": ":0"}, timeout=30.5,
     extra_args=["--proxy=x"], sandbox_bypass=False,
     io={"_key_lock": None, "get_browser": None, "hermes_home_key": "/h/home",
         "_launch_owned": [{"key": "k10", "pid": 11}, None]})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    total = sum(counts.values())
    print("wrote %s: %d cases across %d funcs" % (OUT, total, len(counts)))
    for fn, n in sorted(counts.items()):
        flag = "  <-- TOO FEW" if n < 5 else ""
        print("  %-45s %d%s" % (fn, n, flag))
    if any(n < 5 for n in counts.values()):
        raise SystemExit("every ported function needs >=5 cases")


if __name__ == "__main__":
    main()
