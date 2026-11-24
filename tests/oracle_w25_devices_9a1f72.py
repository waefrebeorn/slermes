#!/usr/bin/env python3
"""Differential oracle for hermes_cli/local_runtime/devices.py (gate w25_devices_9a1f72).

The live module is loaded from the quarry by path (stdlib imports only) and
``probe_devices`` is driven with the child fixed: ``subprocess.run`` is replaced
by a stub that returns the fixture's exit status and stdout, or raises the
fixture's timeout / OSError.  ``_library`` is driven with ``os.name`` fixed.
Compared: the decoded device filter, the library filename selection and the
ggml device-type constants.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_devices_9a1f72"
MODULE = QUARRY / "hermes_cli" / "local_runtime" / "devices.py"


def load_live():
    spec = importlib.util.spec_from_file_location("live_local_runtime_devices", MODULE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


live = load_live()

# The child's launch flags are a platform seam, not a value under test.
compat = types.ModuleType("hermes_cli._subprocess_compat")
compat.windows_hide_flags = lambda: 0
sys.modules["hermes_cli._subprocess_compat"] = compat

WANTED = {"probe_devices", "library", "constants"}


class FakeTimeout(Exception):
    """Stands in for subprocess.TimeoutExpired."""


def fake_subprocess(stdout, returncode, mode):
    def run(*args, **kwargs):
        if mode == "timeout":
            raise FakeTimeout("deadline")
        if mode == "oserror":
            raise OSError("cannot spawn")
        return types.SimpleNamespace(stdout=stdout, returncode=returncode, stderr="")

    return types.SimpleNamespace(DEVNULL=-3, PIPE=-1, run=run, TimeoutExpired=FakeTimeout)


def probe_devices(r):
    stdout = r.get("stdout")
    stdout = stdout if isinstance(stdout, str) else ""
    returncode = r.get("returncode", 0)
    mode = r.get("raise", "none")
    mode = mode if isinstance(mode, str) else "none"
    original = live.subprocess
    live.subprocess = fake_subprocess(stdout, returncode, mode)
    try:
        return live.probe_devices(Path("engine"), "vulkan")
    finally:
        live.subprocess = original


def library(r):
    directory = r.get("directory")
    directory = directory if isinstance(directory, str) else ""
    name = r.get("name")
    name = name if isinstance(name, str) else ""
    nt = bool(r.get("nt", True))
    original = live.os
    live.os = types.SimpleNamespace(name="nt" if nt else "posix")
    try:
        return str(live._library(Path(directory), name)).replace("\\", "/")
    finally:
        live.os = original


def constants():
    return {"ggml_device_gpu": live.GGML_DEVICE_GPU, "ggml_device_igpu": live.GGML_DEVICE_IGPU}


def expected(r):
    f = r.get("func")
    if f == "probe_devices":
        return probe_devices(r)
    if f == "library":
        return library(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
