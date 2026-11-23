#!/usr/bin/env python3
"""Differential oracle for gate w10_startupfa_f8d1b3 (hermes_cli/_startup_fast.py)."""
import importlib.util, json, os, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w10_startupfa_f8d1b3_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "_startup_fast.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def normalize_case(r):
    raw = r.get("raw")
    raw = raw if isinstance(raw, str) else ""
    env_in = r.get("env") or {}
    saved = {}
    for k, v in env_in.items():
        saved[k] = os.environ.get(k)
        os.environ[k] = v
    had = "HERMES_HOME" in os.environ
    prev = os.environ.get("HERMES_HOME")
    os.environ["HERMES_HOME"] = raw
    live.normalize_hermes_home_env()
    after = os.environ.get("HERMES_HOME", "")
    py = after if after != raw else None
    if had:
        os.environ["HERMES_HOME"] = prev
    else:
        os.environ.pop("HERMES_HOME", None)
    for k, v in saved.items():
        if v is None:
            os.environ.pop(k, None)
        else:
            os.environ[k] = v
    return py


def compute(r):
    f = r.get("func")
    if f == "is_global_fast_version_argv":
        return live.is_global_fast_version_argv(r.get("argv"))
    if f == "is_desktop_ssh_backend_argv":
        return live.is_desktop_ssh_backend_argv(r.get("argv"))
    if f == "active_profile_may_override_home":
        original = live._read_text
        live._read_text = lambda path: r.get("active_text")
        try:
            return live.active_profile_may_override_home("ignored")
        finally:
            live._read_text = original
    if f == "normalize_hermes_home_env":
        return normalize_case(r)
    if f == "read_install_method":
        original = live._read_text
        live._read_text = lambda path: r.get("text")
        try:
            return live.read_install_method()
        finally:
            live._read_text = original
    return None


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_startupfa_f8d1b3: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
