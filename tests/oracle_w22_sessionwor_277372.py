#!/usr/bin/env python3
"""Differential oracle for gateway/session_worker_reservation.py (gate
w22_sessionwor_277372).

``verify_worker_identity`` is an owner-side proof over a live process table:
psutil tells it about the Popen handle, the reported worker and the ancestor
chain, and the worker's kernel birth comes from
``hermes_cli.process_identity.worker_birth``.  Those are the OS seams, so the
world the fixture describes is scripted:

  * ``psutil`` is a stub module whose ``Process`` answers the fixture's ppid /
    status / running / ancestors facts and whose ``Error`` is raised when the
    fixture says the lookup failed;
  * ``hermes_cli.process_identity.worker_birth`` is a stub module returning the
    fixture's birth;
  * ``os`` is a namespace supplying the owner's pid/uid/name;
  * the ``process`` object is a real ``subprocess.Popen`` subclass instance
    (``isinstance`` must hold) whose ``poll()`` reports liveness.

Everything else -- the frame validation, the ordering of the refusals, the
launcher-hop check, the tuple answer -- is the live function.

``owner_worker_subject`` runs live with the same ``os`` namespace, and the
reservation calls run live against a recording store: ``authority_persistence``
and ``secrets.token_urlsafe`` are the store/entropy seams, and the returned
asdict/kwargs are what the store would really be handed.
"""
import importlib.util
import json
import subprocess
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "session_worker_reservation.py"
GATE = "w22_sessionwor_277372"
ZOMBIE = "zombie"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("worker_reservation_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["worker_reservation_live"] = live
_spec.loader.exec_module(live)

WANTED = {"verify", "owner_subject", "reserve", "lose", "hops"}


class FakePopen(subprocess.Popen):
    """A Popen that never spawned: only pid and poll() are consulted."""

    def __init__(self, pid, alive):
        self.pid = pid
        self._alive = alive

    def poll(self):
        return None if self._alive else 0


class PseudoError(Exception):
    pass


class PseudoProcess:
    def __init__(self, pid, world, zombie, ppid=None, running=True, parents=()):
        self.pid = pid
        self._world = world
        self._zombie = zombie
        self._ppid = ppid
        self._running = running
        self._parents = list(parents)

    def ppid(self):
        return self._ppid

    def status(self):
        return ZOMBIE if self._zombie else "running"

    def is_running(self):
        return self._running

    def parents(self):
        return [types.SimpleNamespace(pid=p) for p in self._parents]


def make_psutil(world, process_pid, worker_pid):
    module = types.ModuleType("psutil")
    module.STATUS_ZOMBIE = ZOMBIE
    module.Error = PseudoError

    def process(pid):
        if world.get("psutil_error"):
            raise PseudoError("no such process")
        is_handle = process_pid is not None and pid == process_pid
        is_worker = worker_pid is not None and pid == worker_pid
        if not (is_handle or is_worker):
            raise PseudoError("no such process")
        # a direct exec child is both the handle and the worker: one row answers
        # both the ppid/status probe and the birth/running/parents probe.
        zombie = bool((is_handle and world.get("handle_zombie")) or
                      (is_worker and world.get("worker_zombie")))
        return PseudoProcess(
            pid, world, zombie,
            ppid=world.get("handle_ppid") if is_handle else None,
            running=bool(world.get("worker_running", True)) if is_worker else True,
            parents=(world.get("parents") or ()) if is_worker else (),
        )

    module.Process = process
    return module


def make_identity(world):
    module = types.ModuleType("hermes_cli.process_identity")
    module.worker_birth = lambda _process: world.get("worker_birth")
    return module


def verify(world):
    world = world if isinstance(world, dict) else {}
    hello = world.get("hello")
    pid_field = hello.get("pid") if isinstance(hello, dict) else None
    worker_pid = pid_field if isinstance(pid_field, int) and not isinstance(pid_field, bool) else None
    process_pid = world.get("process_pid")
    process = FakePopen(process_pid, bool(world.get("process_live", True)))

    os_ns = types.SimpleNamespace(name=world.get("os_name", "nt"),
                                 getpid=lambda: world.get("owner_pid"),
                                 getuid=lambda: world.get("uid"))
    saved_psutil = sys.modules.get("psutil")
    saved_identity = sys.modules.get("hermes_cli.process_identity")
    saved_os = live.os
    sys.modules["psutil"] = make_psutil(world, process_pid, worker_pid)
    sys.modules["hermes_cli.process_identity"] = make_identity(world)
    live.os = os_ns
    try:
        pid, birth = live.verify_worker_identity(process, hello)
    except live.AuthorityRefusal as exc:
        return {"status": "refused", "reason": str(exc)}
    finally:
        live.os = saved_os
        for name, saved in (("psutil", saved_psutil),
                            ("hermes_cli.process_identity", saved_identity)):
            if saved is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = saved
    return {"status": "verified", "pid": pid, "birth": birth}


def owner_subject(req):
    req = req if isinstance(req, dict) else {}
    os_name = req.get("os_name", "nt")
    os_ns = types.SimpleNamespace(name=os_name,
                                 getpid=lambda: req.get("owner_pid"),
                                 getuid=lambda: req.get("uid"))
    saved_os = live.os
    saved_boot = sys.modules.get("gateway.runtime_bootstrap_windows")
    boot = types.ModuleType("gateway.runtime_bootstrap_windows")
    boot._process_sid = lambda _pid: req.get("sid")
    sys.modules["gateway.runtime_bootstrap_windows"] = boot
    live.os = os_ns
    try:
        return live.owner_worker_subject()
    finally:
        live.os = saved_os
        if saved_boot is None:
            sys.modules.pop("gateway.runtime_bootstrap_windows", None)
        else:
            sys.modules["gateway.runtime_bootstrap_windows"] = saved_boot


class RecordingStore:
    def __init__(self):
        self.calls = []

    def reserve_for_admission(self, **kwargs):
        import dataclasses
        self.calls.append(("reserve", kwargs))
        names = sorted(kwargs)
        Scoped = dataclasses.make_dataclass("Scope", [(n, object) for n in names])
        return Scoped(**kwargs)

    def lose_reservation(self, **kwargs):
        self.calls.append(("lose", kwargs))


def reserve(req):
    req = req if isinstance(req, dict) else {}
    store = RecordingStore()
    authority = types.SimpleNamespace(
        epoch=req.get("epoch"),
        profile_id=req.get("profile_id"),
        _require_admission_open=lambda: None,
    )
    saved_persistence = live.authority_persistence
    saved_rule = live.owner_rule
    saved_secrets = live.secrets
    saved_verify = live.verify_worker_identity
    live.authority_persistence = lambda _authority: types.SimpleNamespace(worker_executions=store)
    live.owner_rule = lambda: req.get("owner")
    live.secrets = types.SimpleNamespace(token_urlsafe=lambda _n: req.get("secret"))
    live.verify_worker_identity = lambda _process, _hello: (req.get("pid"), req.get("birth"))
    try:
        scope = live.reserve_admission_worker(
            authority, admission_id=req.get("admission_id"), process=object(),
            principal_id=req.get("principal_id"), hello={},
            worker_subject=req.get("worker_subject"),
        )
    finally:
        live.authority_persistence = saved_persistence
        live.owner_rule = saved_rule
        live.secrets = saved_secrets
        live.verify_worker_identity = saved_verify
    assert store.calls and store.calls[0][0] == "reserve", store.calls
    return scope


def lose(payload):
    payload = payload if isinstance(payload, dict) else {}
    store = RecordingStore()
    saved_persistence = live.authority_persistence
    live.authority_persistence = lambda _authority: types.SimpleNamespace(worker_executions=store)
    try:
        live.lose_admission_worker(types.SimpleNamespace(), payload.get("row") or {},
                                   payload.get("scope") or {})
    finally:
        live.authority_persistence = saved_persistence
    assert store.calls and store.calls[0][0] == "lose", store.calls
    return store.calls[0][1]


def expected(r):
    f = r.get("func")
    if f == "verify":
        return verify(r.get("world"))
    if f == "owner_subject":
        return owner_subject(r)
    if f == "reserve":
        return reserve(r)
    if f == "lose":
        return lose(r)
    if f == "hops":
        return live.MAX_LAUNCHER_HOPS
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
