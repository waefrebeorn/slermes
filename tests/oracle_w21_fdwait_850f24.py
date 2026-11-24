#!/usr/bin/env python3
"""Differential oracle for tools/environments/fd_wait.py (gate w21_fdwait_850f24).

The live module imports (errno, os, selectors, stat, sys) and is installed by
path; only its OS-facing attributes are swapped per case:

  selectors.DefaultSelector/PollSelector  -> scripted fake selector classes
                                             (raise ValueError / OSError(errno))
  sys.platform                            -> the case's platform
  os.fstat                                -> S_ISCHR for a synthetic st_mode

`open_fd_waiter`'s outcome mapping (ValueError -> None, EBADF -> None, other
OSError -> poll, except a darwin character device which re-raises) and
`FdWaiter.wait`'s `max(timeout, 0)` + `bool(...)` are the live code.
"""
import importlib.util
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tools" / "environments" / "fd_wait.py"
GATE = "w21_fdwait_850f24"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("fd_wait_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["fd_wait_live"] = live
_spec.loader.exec_module(live)

CURRENT = {"select_arg": None, "ready": False}


class FakeSelector:
    """Stands in for selectors.DefaultSelector / PollSelector."""

    def __init__(self):
        self.registered = []
        self.closed = False

    def register(self, fd, events):
        self.registered.append((fd, events))

    def select(self, timeout=None):
        CURRENT["select_arg"] = timeout
        return [1] if CURRENT["ready"] else []

    def close(self):
        self.closed = True


def make_selectors(case):
    """DefaultSelector that fails as the case says; PollSelector always works."""
    reg = case.get("register") or "ok"

    class Default(FakeSelector):
        def __init__(self):
            super().__init__()
            if reg == "ValueError":
                raise ValueError("filedescriptor out of range in select()")
            if reg in ("EBADF", "EMFILE", "other"):
                if reg == "other":
                    code = 9 if (case.get("errno_name") == "EBADF") else 1
                else:
                    code = {"EBADF": 9, "EMFILE": 24}[reg]
                raise OSError(code, "cannot register fd")

    class Poll(FakeSelector):
        pass

    return SimpleNamespace(DefaultSelector=Default, PollSelector=Poll, EVENT_READ=1)


def install(case):
    live.selectors = make_selectors(case)
    live.sys = SimpleNamespace(platform=case.get("platform") or "linux")
    import stat as statmod
    mode = (statmod.S_IFCHR | 0o600) if case.get("is_char_device") else (statmod.S_IFREG | 0o600)
    live.os = SimpleNamespace(fstat=lambda fd: SimpleNamespace(st_mode=mode))
    CURRENT["select_arg"] = None
    CURRENT["ready"] = bool(case.get("ready"))


def expected(r):
    f = r.get("func")
    if f == "clamp":
        t = r.get("timeout")
        return max(t if isinstance(t, (int, float)) else 0, 0)
    install(r)
    if f == "open":
        try:
            waiter = live.open_fd_waiter(1)
        except OSError:
            return {"kind": None, "raised": "OSError"}
        if waiter is None:
            return {"kind": None, "raised": None}
        kind = "select" if type(waiter._selector).__name__ == "Default" else "poll"
        waiter.close()
        return {"kind": kind, "raised": None}
    if f == "wait":
        waiter = live.open_fd_waiter(1)
        result = waiter.wait(r.get("timeout"))
        waiter.close()
        return {"arg": CURRENT["select_arg"], "result": result}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"open", "wait", "clamp"}
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
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
