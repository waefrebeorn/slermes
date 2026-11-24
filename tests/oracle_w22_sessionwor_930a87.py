#!/usr/bin/env python3
"""Differential oracle for gateway/session_worker_delegation.py
(gate w22_sessionwor_930a87).

Everything that needs a live authority connection or a real store stays out of
the C and out of the comparison; the ledger *call* does not. So:

  plan    -- live `persist_worker_delegation` over a recording store handle (the
             store is the DB seam). The expected answer is reconstructed from what
             the call actually did: the store method it invoked, which kwarg
             carried the WorkerDelegationPayload, the seven receipt fields and
             the dispatch-only probe kwargs, plus whether the answer was
             `asdict(...)` rather than `{'value': None}`.
  probe   -- live `_probe` with psutil.Process / process_identity.worker_birth /
             gateway.status.get_process_start_time stubbed (the process seam).
  verb    -- live `_DELIVERY.get(name)`.
  verdict -- the hook's two refusals, transcribed from the source's own
             expressions against the live `gateway.session_worker._SCOPE`.

Receipt fields travel verbatim (int64 values arrive as decimal strings from the
fixtures and are compared as such).
"""
import importlib.util
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "session_worker_delegation.py"
GATE = "w22_sessionwor_930a87"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("session_worker_delegation_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["session_worker_delegation_live"] = live
_spec.loader.exec_module(live)

import psutil  # noqa: E402
import gateway.session_worker as session_worker  # noqa: E402
import gateway.status as gateway_status  # noqa: E402
import hermes_cli.process_identity as process_identity  # noqa: E402

WANTED = {"plan", "probe", "delivery_verb", "request_verdict"}
RECEIPT_KEYS = ("epoch", "execution_id", "session_id", "generation", "sequence",
                "worker_pid", "worker_birth")
PAYLOAD_NAME = "WorkerDelegationPayload"


@dataclass
class LedgerAnswer:
    a: int = 1
    b: str = "x"


class Recorder:
    """The store handle: records the verb and kwargs, answers a ledger row."""

    def __init__(self, value):
        self.calls = []
        self.value = value

    def __getattr__(self, name):
        def method(**kwargs):
            self.calls.append((name, dict(kwargs)))
            return self.value
        return method


class FakeStore:
    def __init__(self, value):
        self.worker_executions = Recorder(value)


def live_probe(probe):
    probe = probe if isinstance(probe, dict) else {}
    alive = probe.get("alive", True)
    birth_equal = probe.get("birth_equal", True)
    started_at = probe.get("started_at")
    alive = True if alive is None else bool(alive)
    birth_equal = True if birth_equal is None else bool(birth_equal)

    orig_process = psutil.Process
    orig_birth = process_identity.worker_birth
    orig_start = gateway_status.get_process_start_time

    def fake_process(pid):
        if not alive:
            raise psutil.Error("gone")
        return SimpleNamespace(pid=pid)

    psutil.Process = fake_process
    process_identity.worker_birth = lambda proc: ("birth" if birth_equal else "other-birth")
    gateway_status.get_process_start_time = lambda pid: started_at
    try:
        started, refusal = live._probe(4242, "birth")
    finally:
        psutil.Process = orig_process
        process_identity.worker_birth = orig_birth
        gateway_status.get_process_start_time = orig_start
    return {"owner_started_at": started, "refusal": refusal}


def live_plan(operation, receipt, probe):
    if not isinstance(receipt, dict):
        return None
    if any(k not in receipt for k in RECEIPT_KEYS):
        # the C contract carries the seven receipt fields; an absent one is out of
        # contract on both sides (upstream it is the caller's own TypeError)
        return None
    store = FakeStore(LedgerAnswer())
    import persistence.workers.delegations  # noqa: F401  (the payload wrapper's home)
    probe_answer = live_probe(probe)
    orig_probe = live._probe
    live._probe = lambda pid, birth: (probe_answer["owner_started_at"], probe_answer["refusal"])
    try:
        result = live.persist_worker_delegation(
            store,
            epoch=receipt["epoch"], execution_id=receipt["execution_id"],
            session_id=receipt["session_id"], generation=receipt["generation"],
            sequence=receipt["sequence"], operation=operation, payload={},
            worker_pid=receipt["worker_pid"], worker_birth=receipt["worker_birth"])
    except Exception:  # AuthorityRefusal('invalid_params')
        return {"ok": False, "reason": "invalid_params"}
    finally:
        live._probe = orig_probe
    calls = store.worker_executions.calls
    if not calls:
        raise AssertionError("no ledger call and no refusal")
    verb, kwargs = calls[0]
    payload_arg = [k for k, v in kwargs.items() if type(v).__name__ == PAYLOAD_NAME]
    kept_receipt = {k: kwargs[k] for k in RECEIPT_KEYS}
    extra = {k: v for k, v in kwargs.items() if k not in RECEIPT_KEYS and k not in payload_arg}
    return {"ok": True, "verb": verb,
            "payload_arg": payload_arg[0] if payload_arg else None,
            "returns_value": result.get("value") is not None,
            "receipt": kept_receipt, "extra": extra}


def delivery_verb(name):
    return live._DELIVERY.get(name)


def request_verdict(keys, has_adopt):
    if not isinstance(keys, list):
        return "invalid_params"
    if set(keys) != session_worker._SCOPE | {"epoch", "sequence", "operation", "payload"}:
        return "invalid_params"
    # the harness's json_get_bool answers true only for a JSON true
    if has_adopt is not True:
        return "permission_denied"
    return "ok"


def expected(r):
    f = r.get("func")
    if f == "plan":
        return live_plan(r.get("operation"), r.get("receipt"), r.get("probe"))
    if f == "probe":
        return live_probe(r.get("probe"))
    if f == "delivery_verb":
        return delivery_verb(r.get("name"))
    if f == "request_verdict":
        return request_verdict(r.get("params_keys"), r.get("has_adopt"))
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
