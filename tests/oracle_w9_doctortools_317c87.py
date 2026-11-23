#!/usr/bin/env python3
"""Differential oracle for hermes_cli/doctor_tools.py (w9_doctortools_317c87 gate).

Environment, PATH lookup and config reads are fixture seams; the live module's
own pure helpers run unmodified for every case.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


def _doctor_check(*args, **kwargs):
    def deco(fn):
        return fn
    return deco


cli = _pkg("hermes_cli")
_mod("hermes_cli.doctor_platform", _system_package_install_cmd=lambda n: f"pkg install {n}")
_mod("hermes_cli.doctor_report",
     Finding=type("Finding", (), {}),
     _fail_and_issue=lambda *a, **k: None,
     check_bool=lambda *a, **k: True,
     check_info=lambda *a, **k: None,
     check_ok=lambda *a, **k: None,
     check_warn=lambda *a, **k: None,
     doctor_check=_doctor_check)
_mod("hermes_cli.vercel_auth", describe_vercel_auth=lambda: None)
_mod("hermes_constants", is_termux=lambda: False)
tools = _pkg("tools")
envs = _pkg("tools.environments")
_mod("tools.environments.docker",
     docker_runtime_name=lambda exe: "Docker",
     docker_runtime_start_hint=lambda exe: "start docker",
     find_docker=lambda: None)
CONFIG = {}


def _cfg_get(config, section, key):
    return (config.get(section) or {}).get(key)


_mod("hermes_cli.config", cfg_get=_cfg_get, load_config_readonly=lambda: CONFIG)

SRC = Q + "/hermes_cli/doctor_tools.py"
spec = importlib.util.spec_from_file_location("w9dt_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w9dt_live"] = live
spec.loader.exec_module(live)

KANBAN_KEY = "HERMES_KANBAN_TASK"
CDP_KEY = "BROWSER_CDP_URL"


def set_env(key, value):
    if value is None:
        os.environ.pop(key, None)
    else:
        os.environ[key] = str(value)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    saved = {k: os.environ.get(k) for k in (KANBAN_KEY, CDP_KEY)}
    real_which = live.shutil.which
    try:
        if func == "plural":
            py = live._plural(int(r.get("n", 0)))
        elif func == "setup_gated":
            py = live._setup_gated(r.get("item"))
        elif func == "kanban_env_gate":
            set_env(KANBAN_KEY, r.get("env_value"))
            py = live._is_kanban_worker_env_gate(r.get("item"))
        elif func == "tool_detail":
            set_env(KANBAN_KEY, r.get("env_value"))
            py = live._doctor_tool_availability_detail(r.get("toolset", ""))
        elif func == "apply_overrides":
            set_env(KANBAN_KEY, r.get("env_value"))
            py = live._apply_doctor_tool_availability_overrides(
                r.get("available"), r.get("unavailable"))
            py = {"available": py[0], "unavailable": py[1]}
        elif func == "missing_api":
            live._enabled_cli_toolsets_for_doctor = lambda: (
                None if r.get("enabled") is None else set(r.get("enabled")))
            py = live._missing_api_key_toolsets_for_summary(r.get("unavailable"))
        elif func == "safe_which":
            path_map = r.get("path_map") or {}
            live.shutil.which = lambda cmd: path_map.get(cmd)
            py = live._safe_which(r.get("cmd", ""))
        elif func == "chromium_reason":
            CONFIG.clear()
            CONFIG.update(r.get("config") or {})
            set_env(CDP_KEY, r.get("cdp_env"))
            py = live._chromium_not_needed_reason()
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    finally:
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
        live.shutil.which = real_which
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w9_doctortools_317c87: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
