#!/usr/bin/env python3
"""Differential oracle for plugins/google_meet/audio_bridge.py
(w20_audiobridg_4173c4).

``platform.system`` and ``subprocess`` are replaced by scripted seams inside
the live module; the class's real setup/teardown/_finish/_ready/_parse_module_id
code runs. Compared values: the returned dict (or RuntimeError), the rollback
call, the unload order and the argv of every process that would be spawned.
"""
import importlib.util
import json
import os
import subprocess
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
NAME = "w20_audiobridg_4173c4_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "google_meet", "audio_bridge.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

CALLS = []


class Fake:
    DEVNULL = subprocess.DEVNULL
    STDOUT = subprocess.STDOUT
    PIPE = subprocess.PIPE
    CalledProcessError = subprocess.CalledProcessError

    def __init__(self, script):
        self.script = script if isinstance(script, dict) else {}

    def run(self, argv, check=True, capture_output=True, text=True,
            encoding=None, errors=None, stdin=None):
        CALLS.append(list(argv))
        if argv[0] != "pactl":
            raise AssertionError("unexpected argv %r" % (argv,))
        if argv[1] == "unload-module":
            if self.script.get("unload_fail"):
                raise OSError("unload refused")
            return types.SimpleNamespace(stdout="", stderr="", returncode=0)
        which = "sink" if "module-null-sink" in argv else "src"
        spec = self.script.get(which) or {}
        if spec.get("missing"):
            raise FileNotFoundError(2, "No such file or directory", "pactl")
        if spec.get("rc", 0) != 0:
            raise subprocess.CalledProcessError(spec.get("rc", 1), list(argv),
                                                stderr=spec.get("stderr") or None)
        return types.SimpleNamespace(stdout=spec.get("stdout", ""), stderr="",
                                     returncode=0)

    def check_output(self, argv, **kw):
        CALLS.append(list(argv))
        spec = self.script.get("profiler") or {}
        if spec.get("missing"):
            raise FileNotFoundError(2, "No such file or directory", "system_profiler")
        if spec.get("rc", 0) != 0:
            raise subprocess.CalledProcessError(spec.get("rc", 1), list(argv),
                                                output=spec.get("out", ""))
        return spec.get("out", "")


def outcome(fn):
    try:
        return {"status": "ok", "value": fn()}
    except Exception as exc:  # noqa: BLE001 - reported verbatim
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}


def expected(r):
    del CALLS[:]
    script = r.get("script") or {}
    live.subprocess = Fake(script)
    system = r.get("system", "Linux")
    live.platform = types.SimpleNamespace(system=lambda: system)
    f = r.get("func")
    if f == "parse_module_id":
        return outcome(lambda: live.AudioBridge._parse_module_id(r.get("stdout", "")))
    if f == "setup":
        bridge = live.AudioBridge(script.get("prefix", "hermes_meet"))
        res = outcome(bridge.setup)
        res["calls"] = [list(c) for c in CALLS]
        return res
    if f == "teardown":
        bridge = live.AudioBridge()
        bridge._module_ids = list(r.get("module_ids") or [])
        bridge.teardown()
        return {"unloaded": [c[2] for c in CALLS]}
    if f == "finish":
        bridge = live.AudioBridge()
        return bridge._finish(r.get("tag", ""), r.get("device", ""),
                              r.get("write_target", ""), list(r.get("module_ids") or []))
    if f == "ready":
        bridge = live.AudioBridge()
        if r.get("value") is not None:
            bridge._device_name = r.get("value")
        return outcome(lambda: bridge.device_name)
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_audiobridg_4173c4: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
