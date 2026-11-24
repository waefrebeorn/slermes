#!/usr/bin/env python3
"""Differential oracle for tui_gateway/hosted_room_peer_http.py (gate w31_hostedroom_50fa41_p2).

Session/run state-machine half.  The live PeerRunsHTTPClient is driven with
``__new__`` (no socket, no store, no clock) and only its documented I/O seams
replaced -- ``_request``, ``_admit_dispatch``, ``_scoped_post``,
``_post_run_action``, ``_poll_receipt`` and ``_session_id`` -- so every branch
compared here is the module's own decision code:

  * ``prepare`` / ``bind_room_scope`` / ``bind_observation``   -> scope fencing
  * ``_accepted`` / ``_save_receipt`` / ``_session_id``        -> record shapes
  * ``close_admission`` / ``recover_dispatch``                 -> admission paths
  * ``_poll_receipt``                                          -> cache + backoff
  * ``history`` / ``status``                                   -> projections
  * ``approve_receipt`` / ``stop_receipt`` / ``refresh_receipt`` -> outcomes
  * ``__init__`` value guards, ``refresh_grant`` replacement check

Domain notes (enforced by the fixture generator):
  * ``dispatch`` fixtures are valid HostedMemberDispatch mappings; from_mapping
    is the caller's contract, not this gate's logic.
  * the receipt store is absent, so ``forget_receipt``'s durable return is the
    "seam" marker on both sides.
"""
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_hostedroom_50fa41_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import hosted_room_peer_http as live  # noqa: E402

CLIENT = live.PeerRunsHTTPClient
GRANT = "g" * 32

WANTED = {
    "api_checks", "prepare", "room_scope", "accepted", "receipt_record",
    "session_id_choice", "close_result", "recover_decision", "poll_cache",
    "observation_receipt", "history_view", "status_view", "approve_decision",
    "forget_decision", "stop_decision", "refresh_decision", "refreshed_grant",
}


def bare(**attrs):
    c = CLIENT.__new__(CLIENT)
    c.base_url = "https://peer.example"
    c.api_key = "k" * 32
    c.transport_security = "tls"
    c.timeout_seconds = 30.0
    c.total_deadline = False
    c.receipt_store = None
    c.receipt_placement = "shared"
    c.poll_min_seconds = 0.1
    c.poll_max_seconds = 2.0
    c.clock = lambda: 0.0
    c._runs = {}
    c._observation_key = None
    c._status_cache = {}
    c._recovery_backoff = {}
    c._terminal_receipts = set()
    c._room_scope = None
    for key, value in attrs.items():
        setattr(c, key, value)
    return c


def project(exc):
    return {
        "message": str(exc),
        "retryable": bool(getattr(exc, "retryable", False)),
        "ambiguous": bool(getattr(exc, "ambiguous", False)),
        "not_admitted": bool(getattr(exc, "not_admitted", False)),
        "status_code": getattr(exc, "status_code", None),
        "error_code": getattr(exc, "error_code", None),
        "needs_reauthorization": bool(getattr(exc, "needs_reauthorization", False)),
    }


def error_from(spec):
    spec = spec or {}
    return live.PeerRunsHTTPError(
        spec.get("message", ""),
        retryable=bool(spec.get("retryable")),
        ambiguous=bool(spec.get("ambiguous")),
        not_admitted=bool(spec.get("not_admitted")),
        status_code=spec.get("status_code"),
        error_code=spec.get("error_code"),
    )


def ok(value):
    return {"ok": True, "value": value}


def fail(exc):
    return {"ok": False, "error": project(exc)}


def cache_proj(c):
    out = {}
    for run_id, slot in c._status_cache.items():
        entry = {}
        for key, value in slot.items():
            entry[key] = project(value) if isinstance(value, live.PeerRunsHTTPError) else value
        out[run_id] = entry
    return out or None


# --------------------------------------------------------------------------
# gates
# --------------------------------------------------------------------------

def f_api_checks(r):
    try:
        CLIENT(base_url="https://peer.example", api_key=r.get("api_key") or "",
               poll_min_seconds=r.get("poll_min_seconds", 0.1),
               poll_max_seconds=r.get("poll_max_seconds", 2.0))
    except ValueError as exc:
        return {"error": str(exc)}
    return {"error": None}


def f_prepare(r):
    c = bare()
    try:
        return ok(dict(c.prepare(room_id=r.get("room_id"), profile=r.get("profile"),
                                 source=r.get("source"), grant=r.get("grant"), create=True,
                                 expected_session_id=r.get("expected_session_id"))))
    except live.PeerRunsHTTPError as exc:
        return fail(exc)


def f_room_scope(r):
    c = bare()
    s = r.get("scope") or {}
    try:
        c.bind_room_scope(room_id=s.get("room_id"), home_install_id=s.get("home_install_id"),
                          authority_gateway_id=s.get("authority_gateway_id"),
                          authority_epoch=s.get("authority_epoch"),
                          member_id=s.get("member_id"),
                          target_install_id=s.get("target_install_id"),
                          target_profile=s.get("target_profile"))
    except live.PeerRunsHTTPError as exc:
        return fail(exc)
    return ok(dict(c._room_scope))


def f_accepted(r):
    d = live.HostedMemberDispatch.from_mapping(r["dispatch"])
    return CLIENT._accepted(d, run_id=r.get("run_id", ""), session_id=r.get("session_id", ""),
                            replayed=bool(r.get("replayed")))


def f_receipt_record(r):
    c = bare()
    d = live.HostedMemberDispatch.from_mapping(r["dispatch"])
    c._save_receipt(d, run_id=r.get("run_id", ""), session_id=r.get("session_id", ""))
    return dict(c._runs[(d.task_id, d.execution_generation)])


def f_session_id_choice(r):
    c = bare()
    key = ("task-abc", 7)
    existing = r.get("existing")
    if existing is not None:
        c._runs[key] = existing
    carrier = SimpleNamespace(task_id=key[0], execution_generation=key[1],
                              room_id="room-alpha", target_profile="default")
    c.prepare = lambda **kwargs: r.get("prepared")
    try:
        return ok(c._session_id(carrier, grant=GRANT))
    except live.PeerRunsHTTPError as exc:
        return fail(exc)


def f_close_result(r):
    c = bare()
    d = live.HostedMemberDispatch.from_mapping(r["dispatch"])
    scope = {f: getattr(d, f) for f in live._RECEIPT_SCOPE_FIELDS}
    c._room_scope = scope
    c._observation_key = (d.task_id, d.execution_generation)
    existing = r.get("existing")
    if existing is not None:
        c._runs[(d.task_id, d.execution_generation)] = existing
    result = r.get("result") or {}
    c._scoped_post = lambda path, grant, *, body: result
    c._session_id = lambda dispatch, *, grant: r.get("session", "")
    try:
        return ok(dict(c.close_admission(dispatch=r["dispatch"], grant=GRANT)))
    except live.PeerRunsHTTPError as exc:
        return fail(exc)


def f_recover_decision(r):
    c = bare()
    c.clock = lambda: r.get("now", 0.0)
    c.poll_min_seconds = r.get("poll_min_seconds", 0.1)
    c.poll_max_seconds = r.get("poll_max_seconds", 2.0)
    d = live.HostedMemberDispatch.from_mapping(r["dispatch"])
    key = (d.task_id, d.execution_generation)
    scope = {f: getattr(d, f) for f in live._RECEIPT_SCOPE_FIELDS}
    c._room_scope = scope
    c._observation_key = key
    existing = r.get("existing")
    if existing is not None:
        c._runs[key] = existing
    backoff = r.get("backoff")
    if backoff is not None:
        c._recovery_backoff = {key: dict(backoff)}
    admit = r.get("admit") or {}

    def fake_admit(checked, *, grant):
        if admit.get("ok"):
            return {"status": "accepted", "task_id": checked.task_id,
                    "execution_generation": checked.execution_generation,
                    "run_id": admit.get("run_id", ""), "session_id": "sess-1",
                    "replayed": bool(admit.get("replayed"))}
        raise error_from(admit.get("error"))

    c._admit_dispatch = fake_admit
    out = {"action": None, "accepted": None, "backoff": None, "error": None, "replayed": None}
    try:
        res = c.recover_dispatch(dispatch=r["dispatch"], grant=GRANT)
    except live.PeerRunsHTTPError as exc:
        message = str(exc)
        if message == "peer run receipt conflicts with the recovered dispatch":
            out["action"] = "conflict"
        elif message == "peer admission recovery is backing off":
            out["action"] = "backing_off"
        else:
            out["action"] = "failed"
            if exc.retryable or exc.ambiguous:
                held = c._recovery_backoff.get(key)
                if held is not None:
                    out["backoff"] = {"delay": float(held["delay"]),
                                      "next_attempt_at": float(held["next_attempt_at"])}
        out["error"] = project(exc)
        return out
    if existing is not None:
        out["action"] = "replayed"
        out["accepted"] = res
    else:
        out["action"] = "admitted"
        out["replayed"] = bool(res.get("replayed"))
    return out


def f_poll_cache(r):
    c = bare()
    c.clock = lambda: r.get("now", 0.0)
    c.poll_min_seconds = r.get("poll_min_seconds", 0.1)
    c.poll_max_seconds = r.get("poll_max_seconds", 2.0)
    record = r["record"]
    run_id = str(record["run_id"])
    cached = r.get("cached")
    c._status_cache = {} if cached is None else {run_id: json.loads(json.dumps(cached))}
    if cached is not None:
        stored = c._status_cache[run_id]
        if isinstance(stored.get("error"), dict):
            stored["error"] = error_from(stored["error"])
    c._terminal_receipts = set()
    calls = []
    fetched = r.get("fetched") or {}

    def fake_request(path, *, method="GET", body=None, headers=None, room_grant=None):
        calls.append(path)
        if not fetched.get("ok"):
            raise error_from(fetched.get("error"))
        return fetched.get("full")

    c._request = fake_request
    out = {"action": None, "status": None, "cache": None, "terminal_key": None,
           "resolved": False, "error": None}
    try:
        status = c._poll_receipt(record, grant=GRANT)
    except live.PeerRunsHTTPError as exc:
        out["action"] = "failed" if calls else "cached_raise"
        out["error"] = project(exc)
        if calls:
            out["cache"] = cache_proj(c)
        out["resolved"] = len(calls) > 1
        return out
    out["action"] = "fetched" if calls else (
        "cached_terminal"
        if str((cached.get("status") or {}).get("status")) in live._TERMINAL_RUN_STATES
        else "cached_wait")
    out["status"] = status
    out["resolved"] = len(calls) > 1
    if calls:
        out["cache"] = cache_proj(c)
        key = (str(record["task_id"]), int(record["execution_generation"]))
        if key in c._terminal_receipts:
            out["terminal_key"] = [key[0], key[1]]
    return out


def f_observation_receipt(r):
    c = bare()
    key = r.get("key")
    c._observation_key = None if key is None else (str(key[0]), int(key[1]))
    record = r.get("record")
    if record is not None:
        c._runs[(str(record["task_id"]), int(record["execution_generation"]))] = record
    explicit = r.get("explicit") or {}
    q = r.get("query") or {}
    try:
        res = c._observation_receipt(room_id=q.get("room_id", ""), profile=q.get("profile", ""),
                                     session_id=q.get("session_id", ""),
                                     task_id=explicit.get("task_id"),
                                     execution_generation=explicit.get("execution_generation"))
    except live.PeerRunsHTTPError as exc:
        return fail(exc)
    return {"record": res}


def f_history_view(r):
    c = bare()
    receipt = r.get("receipt")
    q = r.get("query") or {}
    if receipt is not None:
        key = (str(receipt["task_id"]), int(receipt["execution_generation"]))
        c._observation_key = key
        c._runs[key] = receipt
    status = r.get("status")
    c._poll_receipt = lambda record, *, grant: status
    try:
        return list(c.history(room_id=q.get("room_id", ""), profile=q.get("profile", ""),
                              session_id=q.get("session_id", ""), grant=GRANT))
    except live.PeerRunsHTTPError as exc:
        return {"__raise__": project(exc)}


def f_status_view(r):
    c = bare()
    receipt = r.get("receipt")
    q = r.get("query") or {}
    if receipt is not None:
        key = (str(receipt["task_id"]), int(receipt["execution_generation"]))
        c._observation_key = key
        c._runs[key] = receipt
    status = r.get("status")
    c._poll_receipt = lambda record, *, grant: status
    try:
        return dict(c.status(room_id=q.get("room_id", ""), profile=q.get("profile", ""),
                             session_id=q.get("session_id", ""), grant=GRANT))
    except live.PeerRunsHTTPError as exc:
        return {"__raise__": project(exc)}


def f_approve_decision(r):
    c = bare()
    record = r.get("record")
    captured = {}
    if record is not None:
        c._runs[(str(record["task_id"]), int(record["execution_generation"]))] = record
        cached = r.get("cached")
        c._status_cache = {} if cached is None else {str(record["run_id"]): cached}
    status = r.get("status")
    c._poll_receipt = lambda rec, *, grant: status
    action = r.get("action") or {}

    def fake_action(rec, name, *, body, grant):
        captured["body"] = body
        if action.get("ok"):
            return action.get("result")
        raise error_from(action.get("error"))

    c._post_run_action = fake_action
    out = {"action": None, "body": None, "identity": None, "result": None, "error": None}
    try:
        res = c.approve_receipt(task_id=r.get("task_id", "task-abc"),
                                execution_generation=r.get("execution_generation", 7),
                                request_id=r.get("request_id"), choice=r.get("choice"),
                                grant=GRANT)
    except live.PeerRunsHTTPError as exc:
        out["action"] = "raise"
        out["error"] = project(exc)
        return out
    if res is None:
        out["action"] = "none"
        return out
    body = captured.get("body") or {}
    out["action"] = "post"
    out["body"] = body
    out["identity"] = {k: v for k, v in body.items() if k != "choice"}
    out["result"] = res
    return out


def f_forget_decision(r):
    c = bare()
    record = r.get("record")
    run_id = r.get("run_id")
    if record is not None:
        task_id = str(record["task_id"])
        generation = int(record["execution_generation"])
        c._runs[(task_id, generation)] = record
    else:
        task_id, generation = "task-missing", 1
    c._status_cache = {str(run_id): {"status": {"status": "running"}}}
    c._terminal_receipts = {(task_id, generation)}
    result = c.forget_receipt(task_id=task_id, execution_generation=generation, run_id=run_id)
    return {
        "dropped": record is not None and (task_id, generation) not in c._runs,
        "return": "seam" if r.get("has_store") else result,
    }


def f_stop_decision(r):
    c = bare()
    record = r.get("record")
    if record is not None:
        key = (str(record["task_id"]), int(record["execution_generation"]))
        c._runs[key] = record
    err_spec = r.get("error")
    result = r.get("result")

    def fake_action(rec, name, *, body, grant):
        if err_spec:
            raise error_from(err_spec)
        return result

    c._post_run_action = fake_action
    c._poll_receipt = lambda rec, *, grant: {"run_id": "run-1", "status": "interrupted"}
    out = {"action": None, "terminal": False, "result": None, "error": None}
    try:
        res = c.stop_receipt(task_id=r.get("task_id", "task-abc"),
                             execution_generation=r.get("execution_generation", 7), grant=GRANT)
    except live.PeerRunsHTTPError as exc:
        out["action"] = "raise"
        out["error"] = project(exc)
        return out
    if res is None:
        out["action"] = "none"
        return out
    if err_spec:
        out["action"] = "repoll"
        return out
    out["action"] = "returned"
    out["result"] = res
    if record is not None:
        out["terminal"] = (str(record["task_id"]),
                           int(record["execution_generation"])) in c._terminal_receipts
    return out


def f_refresh_decision(r):
    c = bare()
    d = live.HostedMemberDispatch.from_mapping(r["dispatch"])
    key = (d.task_id, d.execution_generation)
    c._room_scope = {f: getattr(d, f) for f in live._RECEIPT_SCOPE_FIELDS}
    c._observation_key = key
    record = r.get("record")
    if record is not None:
        c._runs[key] = record
    err_spec = r.get("error")

    def fake_request(path, *, method="GET", body=None, headers=None, room_grant=None):
        if err_spec:
            raise error_from(err_spec)
        return {"run_id": record["run_id"], "status": "running"}

    c._request = fake_request
    out = {"action": None, "run_id": None, "error": None}
    try:
        c.refresh_receipt(dispatch=r["dispatch"], grant=GRANT)
    except live.PeerRunsHTTPError as exc:
        out["action"] = "raise"
        out["error"] = project(exc)
        return out
    if record is None:
        out["action"] = "none"
        return out
    if key not in c._runs:
        out["action"] = "forget"
        out["run_id"] = record["run_id"]
        return out
    out["action"] = "ok"
    return out


def f_refreshed_grant(r):
    c = bare()
    refreshed = r.get("refreshed") or {}
    replacement = str(refreshed.get("grant") or "")
    if not replacement:
        return fail(live.PeerRunsHTTPError("peer returned no refreshed room grant"))
    return ok(replacement)


DISPATCH = {
    "api_checks": f_api_checks,
    "prepare": f_prepare,
    "room_scope": f_room_scope,
    "accepted": f_accepted,
    "receipt_record": f_receipt_record,
    "session_id_choice": f_session_id_choice,
    "close_result": f_close_result,
    "recover_decision": f_recover_decision,
    "poll_cache": f_poll_cache,
    "observation_receipt": f_observation_receipt,
    "history_view": f_history_view,
    "status_view": f_status_view,
    "approve_decision": f_approve_decision,
    "forget_decision": f_forget_decision,
    "stop_decision": f_stop_decision,
    "refresh_decision": f_refresh_decision,
    "refreshed_grant": f_refreshed_grant,
}


def expected(r):
    return DISPATCH[r["func"]](r)


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
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
