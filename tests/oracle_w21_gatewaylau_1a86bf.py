#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_launchd_runtime.py
(gate w21_gatewaylau_1a86bf).

The module is macOS-only and shells out to `launchctl`, `pgrep` and `osascript`,
so the oracle lifts its real definitions out of the source and runs them against
scripted seams -- a clock, a `_launchctl`, a `subprocess`, a `uuid4`, an
`os.open`/`fdopen` and a `plistlib.dump` that records instead of writing:

* `_budget` is the live function over the fake monotonic clock;
* `GatewayJob` is the live class; one case is one `poll()` call with the job's
  recorded state (`_status`, `_checked`, `pid`) preloaded, so the interval gate,
  the `pid = N` / `last exit code = N` scan and the pgrep fallback are all the
  module's own code;
* `_remove_finished_jobs` is the live function over a scripted `launchctl list`;
* `start_gateway_job` is the live function; the captured plist is the real
  `plistlib.dump` argument, minus `ProgramArguments`
  (`gateway_launchd.launchd_program_arguments` is a different module).
"""
import ast
import importlib.util
import json
import re
import subprocess
import sys
import uuid
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "gateway_launchd_runtime.py"
GATE = "w21_gatewaylau_1a86bf"
_ANN = __import__("__future__").annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


class Clock:
    def __init__(self):
        self.t = 1000.0

    def monotonic(self):
        return self.t


CLOCK = Clock()

STATE = {"launchctl": None, "pgrep": [], "pgrep_fail": False, "calls": []}


def fake_launchctl(*args, timeout):
    STATE["calls"].append((args, timeout))
    return STATE["launchctl"]


def fake_run(cmd, **kwargs):
    if STATE["pgrep_fail"]:
        raise OSError("pgrep missing")
    return SimpleNamespace(returncode=0, stdout=" ".join(STATE["pgrep"]), stderr="")


class FakeOS:
    O_WRONLY = 1
    O_CREAT = 2
    O_EXCL = 4

    def __init__(self):
        self.uid = 501

    def getuid(self):
        return self.uid

    def open(self, *args, **kwargs):
        return -1

    def fdopen(self, fd, mode):
        class Handle:
            def __enter__(self):
                return self

            def __exit__(self, *exc):
                return False

        return Handle()


class FakePlist:
    def __init__(self):
        self.dumped = []

    def dump(self, payload, handle):
        self.dumped.append(payload)


class FakeUuid4:
    def __init__(self, hexstr):
        self.hex = hexstr


class FakeUuid:
    def __init__(self):
        self.hexstr = "0" * 32

    def uuid4(self):
        return FakeUuid4(self.hexstr)


FAKE_OS = FakeOS()
FAKE_PLIST = FakePlist()
FAKE_UUID = FakeUuid()


class FakeSub:
    def __init__(self):
        self.DEVNULL = -3
        self.TimeoutExpired = subprocess.TimeoutExpired
        self.CompletedProcess = subprocess.CompletedProcess
        self.run = staticmethod(fake_run)


G = {
    "__name__": "gateway_launchd_runtime_live",
    "re": re,
    "time": CLOCK,
    "os": FAKE_OS,
    "plistlib": FAKE_PLIST,
    "uuid": FAKE_UUID,
    "subprocess": FakeSub(),
    "Path": Path,
    "int": int,
    "str": str,
    "dict": dict,
    "list": list,
    "None": None,
    "_launchctl": fake_launchctl,
}

WANTED_FUNCS = {"_budget", "_remove_finished_jobs", "start_gateway_job"}
WANTED_CLASSES = {"GatewayJob"}
WANTED_ASSIGNS = {"LABEL_PREFIX", "_POLL_INTERVAL_S"}

_tree = ast.parse(SRC.read_text(encoding="utf-8"))
for _node in _tree.body:
    if isinstance(_node, ast.Assign):
        _names = [t.id for t in _node.targets if isinstance(t, ast.Name)]
        if any(n in WANTED_ASSIGNS for n in _names):
            exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), G)
    elif isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in WANTED_FUNCS:
        _copy = ast.Module(body=[_node], type_ignores=[])
        exec(compile(_copy, str(SRC), "exec", flags=_ANN, dont_inherit=True), G)
    elif isinstance(_node, ast.ClassDef) and _node.name in WANTED_CLASSES:
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec",
                     flags=_ANN, dont_inherit=True), G)

LABEL_PREFIX = G["LABEL_PREFIX"]


def expected_budget(r):
    CLOCK.t = float(r.get("now") or 0.0)
    deadline = r.get("deadline")
    deadline = None if deadline is None else float(deadline)
    return {"value": G["_budget"](deadline, float(r.get("cap") or 0.0))}


def expected_poll(r):
    CLOCK.t = 1000.0
    budget = r.get("budget")
    deadline = None if budget is None else CLOCK.t + float(budget)
    job = G["GatewayJob"]("gui/501/label", deadline)
    job._status = r.get("prev_status")
    job._checked = CLOCK.t - float(r.get("elapsed") or 0.0)
    job.pid = r.get("pid")
    STATE["calls"] = []
    STATE["launchctl"] = SimpleNamespace(returncode=int(r.get("rc") or 0),
                                         stdout=r.get("stdout") or "")
    STATE["pgrep"] = [str(x) for x in (r.get("pgrep") or [])]
    STATE["pgrep_fail"] = bool(r.get("pgrep_fail"))
    status = job.poll()
    return {"status": status, "pid": job.pid, "checked": bool(STATE["calls"])}


def expected_finished(r):
    removed = []
    STATE["launchctl"] = SimpleNamespace(returncode=int(r.get("rc") or 0),
                                         stdout=r.get("stdout") or "")

    def recording_launchctl(*args, timeout):
        if args and args[0] == "remove":
            removed.append(args[1])
        return STATE["launchctl"]

    G["_launchctl"] = recording_launchctl
    try:
        G["_remove_finished_jobs"](None)
    finally:
        G["_launchctl"] = fake_launchctl
    return removed


def expected_job(r):
    FAKE_PLIST.dumped = []
    FAKE_UUID.hexstr = r.get("hex12") or ""
    FAKE_OS.uid = int(r.get("uid") or 0)
    STATE["launchctl"] = SimpleNamespace(returncode=0, stdout="")
    job = G["start_gateway_job"](list(r.get("command") or []), env=r.get("env") or {},
                                 cwd=r.get("cwd") or "", log_path=r.get("log_path") or "x.log",
                                 deadline=None)
    plist = FAKE_PLIST.dumped[0]
    label = plist["Label"]
    out = {
        "label": label,
        "target": job.target if job else None,
        "plist_name": ".%s.plist" % label,
        "wrapped": ["/usr/bin/env", "XPC_SERVICE_NAME=0", *list(r.get("command") or [])],
        "plist": {k: v for k, v in plist.items() if k != "ProgramArguments"},
    }
    return out


def expected(r):
    f = r.get("func")
    if f == "budget":
        return expected_budget(r)
    if f == "poll":
        return expected_poll(r)
    if f == "finished_jobs":
        return expected_finished(r)
    if f == "job":
        return expected_job(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"budget", "poll", "finished_jobs", "job"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
