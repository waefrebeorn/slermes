#!/usr/bin/env python3
"""Differential oracle for gateway/runtime_bootstrap_windows.py (gate w34_runtimeboo_7f06de).

The live module is imported as a package module and its pure decision core is
called on plain values; every Win32 seam is either handed in as an argument or
replaced by a scripted stand-in for the duration of one case:

  * ``classify_refused_owner`` / ``refused_owner_remedy`` / ``judge_pipe_server``
    are called directly -- they already take the probed facts as arguments;
  * ``_remaining_ms`` is called with ``time.monotonic`` pinned, so the value is
    a function of the remaining seconds alone;
  * ``_accept``'s tail and ``query_runtime_control``'s open/wait error table are
    driven through the live code with a fake ``_native`` (ConnectNamedPipe /
    CreateFile / WaitNamedPipe), so the 995/231/121 policy is the live one;
  * ``_read_line``'s body runs against a scripted overlapped read, which reports
    the chunk size it was asked for and the size-limit refusal;
  * ``_peer_subject``'s serving half runs against a fake kernel32 and fake
    process lookups;
  * ``_create_pipe``'s DACL string and open mode are literals inside a function
    that calls advapi32, so those two are mirrored from the source expression
    (``'D:P(A;;GA;;;' + sid + ')'`` and ``3 | FILE_FLAG_OVERLAPPED |
    (0x00080000 if first else 0)``) rather than called.

The C harness prints its computed value as "out" next to the echoed inputs;
only "out" is compared, with Python equality.
"""
import ctypes
import json
import sys
import time
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_runtimeboo_7f06de"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway import runtime_bootstrap_windows as live  # noqa: E402

WANTED = {
    "classify_refused_owner", "refused_owner_remedy", "judge_pipe_server",
    "peer_subject", "remaining_ms", "accept_outcome", "pipe_open_action",
    "read_step", "pipe_dacl", "pipe_create_mode", "constants",
}


# -- direct calls ----------------------------------------------------------

def classify_refused_owner(r):
    return live.classify_refused_owner(
        own_session=r.get("own_session"),
        owner_session=r.get("owner_session"),
        own_elevated=bool(r.get("own_elevated")),
        owner_elevated=r.get("owner_elevated"),
    )


def refused_owner_remedy(r):
    return live.refused_owner_remedy(r["reason"], r["pid"], r.get("owner_session"))


def judge_pipe_server(r):
    pipe = r.get("pipe")
    if pipe is not None:
        pipe = (pipe.get("owner"),
                tuple((ace[0], ace[1]) for ace in (pipe.get("dacl") or [])))
    pid = r.get("pid")
    try:
        subject = live.judge_pipe_server(
            own_sid=r["own_sid"],
            process_sid=r.get("process_sid"),
            pipe=pipe,
            pid=pid,
            own_elevated=bool(r.get("own_elevated")),
            own_session=r.get("own_session"),
            server_session=r.get("server_session"),
            server_elevated=r.get("server_elevated"),
        )
    except live.RefusedOwner as exc:
        return {"ok": False, "subject": None, "error": str(exc),
                "reason": exc.reason_code, "pid": exc.pid}
    except PermissionError as exc:
        return {"ok": False, "subject": None, "error": str(exc),
                "reason": None, "pid": pid}
    return {"ok": True, "subject": subject, "error": None, "reason": None, "pid": pid}


# -- scripted seams --------------------------------------------------------

class _FakeFn:
    def __init__(self, impl):
        self.impl = impl

    def __call__(self, *args, **kwargs):
        return self.impl(*args, **kwargs)


class _FakeDll:
    def __init__(self, **fns):
        self.fns = {name: _FakeFn(fn) for name, fn in fns.items()}

    def __getattr__(self, name):
        return self.fns[name]


class _FakeOv:
    def __init__(self, payload=b"", error=0):
        self.event = object()
        self.payload = payload
        self.error = error

    def GetOverlappedResult(self, wait):
        return (len(self.payload), self.error)

    def getbuffer(self):
        return self.payload

    def cancel(self):
        pass


def peer_subject(r):
    """The serving half of ``_peer_subject``: client token, then client PID."""
    saved_ctypes = live.ctypes
    saved_client = live._pipe_client_sid
    saved_process = live._process_sid
    saved_getpid = live.os.getpid
    client_sid = r.get("client_sid")
    pid_sid = r.get("pid_sid")
    own_sid = r.get("own_sid")
    client_pid = 4242
    handles = {}

    def fake_client_sid(handle):
        if client_sid is None:
            raise PermissionError("cannot impersonate named-pipe peer")
        return client_sid

    def fake_process_sid(pid):
        if pid == client_pid:
            if pid_sid is None:
                raise PermissionError("cannot verify bootstrap peer process")
            return pid_sid
        return own_sid

    def fake_get_pid(handle, byref_pid):
        if not r.get("client_pid"):
            return 0
        byref_pid._obj.value = client_pid
        handles["pid"] = client_pid
        return 1

    def fake_windll(name, **kwargs):
        return _FakeDll(GetNamedPipeClientProcessId=fake_get_pid,
                        GetNamedPipeServerProcessId=fake_get_pid)

    live.ctypes = types.SimpleNamespace(WinDLL=fake_windll, byref=ctypes.byref,
                                        POINTER=ctypes.POINTER, c_int=ctypes.c_int,
                                        c_void_p=ctypes.c_void_p)
    live._pipe_client_sid = fake_client_sid
    live._process_sid = fake_process_sid
    live.os.getpid = lambda: 1
    try:
        try:
            subject = live._peer_subject(0, server=False)
        except PermissionError as exc:
            return {"ok": False, "subject": None, "error": str(exc)}
        return {"ok": True, "subject": subject, "error": None}
    finally:
        live.ctypes = saved_ctypes
        live._pipe_client_sid = saved_client
        live._process_sid = saved_process
        live.os.getpid = saved_getpid


def remaining_ms(r):
    saved_time = live.time
    live.time = types.SimpleNamespace(monotonic=lambda: 1000.0)
    try:
        try:
            ms = live._remaining_ms(1000.0 + r.get("remaining", 0.0))
        except TimeoutError as exc:
            return {"ok": False, "ms": None, "error": str(exc)}
        return {"ok": True, "ms": ms, "error": None}
    finally:
        live.time = saved_time


def accept_outcome(r):
    code = r.get("error")

    class Win:
        WAIT_OBJECT_0 = 0

        @staticmethod
        def ConnectNamedPipe(handle, overlapped=True):
            return _FakeOv(b"", code)

        @staticmethod
        def WaitForSingleObject(event, ms):
            return 0

    saved = live._native
    live._native = lambda: Win
    try:
        try:
            connected = live._accept(0, 0.5)
        except OSError as exc:
            return {"connected": False, "error": exc.strerror, "code": exc.winerror}
        return {"connected": bool(connected), "error": None,
                "code": None if connected else code}
    finally:
        live._native = saved


class _WaitReached(Exception):
    pass


def pipe_open_action(r):
    """Drive the live CreateFile/WaitNamedPipe loop far enough to classify."""
    code = r.get("code")
    phase = r.get("phase") or "open"
    open_code = 231 if phase == "wait" else code

    class Win:
        NULL = 0
        OPEN_EXISTING = 3
        FILE_FLAG_OVERLAPPED = 0x40000000

        @staticmethod
        def CreateFile(*args):
            raise OSError(open_code, "open failed", None, open_code)

        @staticmethod
        def WaitNamedPipe(name, ms):
            if phase == "wait":
                raise OSError(code, "wait failed", None, code)
            raise _WaitReached()

    saved = live._native
    saved_pipe = sys.modules.get("gateway.control_socket")
    stub = types.ModuleType("gateway.control_socket")
    stub.windows_pipe_name = lambda home: r"\\.\pipe\w34-test"
    sys.modules["gateway.control_socket"] = stub
    live._native = lambda: Win
    try:
        live.query_runtime_control(Path("C:/w34home"), b"ping\n", 5.0)
    except _WaitReached:
        return {"action": "wait", "error": None}
    except FileNotFoundError as exc:
        return {"action": "fail", "error": str(exc)}
    except PermissionError as exc:
        return {"action": "fail", "error": str(exc)}
    except TimeoutError as exc:
        return {"action": "fail", "error": str(exc)}
    except OSError:
        return {"action": "raise", "error": None}
    finally:
        live._native = saved
        if saved_pipe is not None:
            sys.modules["gateway.control_socket"] = saved_pipe
        else:
            sys.modules.pop("gateway.control_socket", None)
    # Unreachable: CreateFile always raises, so the loop cannot fall through.
    raise AssertionError("query_runtime_control returned without an outcome")


class _StopRead(Exception):
    pass


def read_step(r):
    """Run one live _read_line iteration over a scripted overlapped read."""
    maximum = r.get("maximum")
    before = r.get("before", 0)
    count = r.get("count", 0)
    win = types.SimpleNamespace()
    win.sizes = []
    win.payloads = ([b"z" * before] if before else []) + [b"y" * count]

    def ReadFile(handle, size, overlapped=True):
        win.sizes.append(size)
        if win.payloads:
            return _FakeOv(win.payloads.pop(0), 0), None
        # Nothing refused: the next request size was recorded; stop here.
        raise _StopRead()

    win.ReadFile = ReadFile
    win.WAIT_OBJECT_0 = 0
    win.WaitForSingleObject = lambda event, ms: 0
    saved = live._native
    live._native = lambda: win
    try:
        live._read_line(0, time.monotonic() + 2.0, maximum)
    except (PermissionError, ConnectionError) as exc:
        return {"chunk": None, "error": str(exc)}
    except _StopRead:
        return {"chunk": win.sizes[-1], "error": None}
    finally:
        live._native = saved
    raise AssertionError("_read_line returned without a newline")


# -- inline literals mirrored from the source ------------------------------

def pipe_dacl(r):
    return "D:P(A;;GA;;;" + r.get("sid", "") + ")"


def pipe_create_mode(r):
    return 3 | 0x40000000 | (0x00080000 if r.get("first") else 0)


def constants():
    return {
        "sqos": live._PIPE_CLIENT_SQOS,
        "pipe_instances": live._PIPE_INSTANCES,
        "client_open_flags": 0x40000000 | live._PIPE_CLIENT_SQOS,
    }


HANDLERS = {
    "classify_refused_owner": classify_refused_owner,
    "refused_owner_remedy": refused_owner_remedy,
    "judge_pipe_server": judge_pipe_server,
    "peer_subject": peer_subject,
    "remaining_ms": remaining_ms,
    "accept_outcome": accept_outcome,
    "pipe_open_action": pipe_open_action,
    "read_step": read_step,
    "pipe_dacl": pipe_dacl,
    "pipe_create_mode": pipe_create_mode,
    "constants": lambda r: constants(),
}


def expected(r):
    return HANDLERS[r.get("func")](r)


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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
