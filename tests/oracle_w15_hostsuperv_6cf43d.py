#!/usr/bin/env python3
"""Differential oracle for tui_gateway/host_supervisor.py (gate w15_hostsuperv_6cf43d).

Loads the LIVE module with stubbed hermes_constants / hermes_cli._startup_fast /
tools.environments.local (their real imports pull in the whole agent stack).
Compares three deterministic decisions with the C harness `out`:
  is_compute_host_identity (command string injected),
  append_log_record (text normalization, via a real temp-file write),
  HostSupervisor._validate_hello (hello facts injected on a bare instance).
"""
import importlib.util
import json
import os
import sys
import tempfile
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _seed(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], m)
    return m


_seed("hermes_constants", get_hermes_home=lambda: types.SimpleNamespace(
    __str__=lambda self: "/home/hermes"))
_seed("hermes_cli")
_seed("hermes_cli._startup_fast", project_root_str=lambda: QUARRY)
_seed("tools")
_seed("tools.environments")
_seed("tools.environments.local", hermes_subprocess_env=lambda **kw: {})

_spec = importlib.util.spec_from_file_location(
    "hs_live", QUARRY + "/tui_gateway/host_supervisor.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["hs_live"] = live
_spec.loader.exec_module(live)

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
    try:
        if func == "identity":
            cmd = r.get("command", "")
            live._pid_command = lambda pid: cmd
            py = live.is_compute_host_identity(0)
        elif func == "log_text":
            fd, path = tempfile.mkstemp(suffix=".log")
            os.close(fd)
            try:
                fakeos = types.SimpleNamespace(
                    name="posix", open=os.open, O_WRONLY=os.O_WRONLY,
                    O_CREAT=os.O_CREAT, O_APPEND=os.O_APPEND,
                    write=os.write, close=os.close)
                live.os = fakeos
                live.append_log_record(path, r.get("record", ""))
                with open(path, "rb") as fh:
                    # os.open() without O_BINARY is text mode on Windows, so the
                    # live write expands LF to CRLF; compare the logical record.
                    py = fh.read().decode("utf-8", "replace").replace("\r\n", "\n")
            finally:
                live.os = os
                try:
                    os.unlink(path)
                except OSError:
                    pass
        elif func == "hello":
            sup = object.__new__(live.HostSupervisor)
            sup._hello = ({} if not r.get("hello_present") else
                          {"hermes_home": r.get("hello_home", ""),
                           "build_sha": r.get("hello_sha", "")})
            sup.expected_hermes_home = r.get("expected_home", "")
            sup.expected_build_sha = r.get("expected_sha", "")
            try:
                sup._validate_hello()
                py = None
            except RuntimeError as exc:
                py = str(exc)
        else:
            continue
    except Exception as e:
        print("PY-ERROR %r: %s" % (r, e), file=sys.stderr)
        mism += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w15_hostsuperv_6cf43d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
