#!/usr/bin/env python3
"""Differential oracle for tui_gateway/hosted_room_peer_http.py (gate w31_hostedroom_50fa41_p1).

Request building + error classification half.  The live module is imported by
path from the quarry tree and its own functions are called: the socket seam is
the credentialed opener (`hermes_cli.urllib_security.open_credentialed_url`),
which this oracle replaces with a fixture-driven fake; the clock is a fixture
value.  Everything else is the module's real code:

  * ``_valid_code`` / ``_response_error_code``      -> error-code extraction
  * ``_run_path`` / ``_run_status``                 -> URL path + status view
  * ``_next_poll_delay`` / ``_require_room_grant``  -> backoff + grant predicate
  * ``PeerRunsHTTPClient._request``                 -> URL/headers/body + parse
  * ``PeerRunsHTTPClient._raise_http_error``        -> HTTP status classifier
  * ``_read_body``                                  -> byte/time budget mapping
  * ``_is_proven_pre_admission_failure``            -> unreachable verdict
  * ``digest_reauthorization_error``                -> frozen digest drift
  * ``issue_invitation`` / ``_admit_dispatch``      -> request bodies

Domain notes (enforced by the fixture generator):
  * ASCII request bodies carrying only strings/ints, so the C serializer and
    ``json.dumps(separators=(",", ":"))`` agree byte for byte.
  * The response carrier models read1/Content-Length only: raw bytes plus an
    optional declared length (the chunking loop itself is a seam).
  * errno membership travels by NAME, as upstream's getattr(errno, name) filter
    is platform-local.
  * A dispatch fixture is always a valid HostedMemberDispatch (from_mapping is
    the caller's contract, not this gate's logic).
"""
import errno
import io
import json
import socket
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_hostedroom_50fa41_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import hosted_room_peer_http as live  # noqa: E402
import hermes_cli.urllib_security as url_security  # noqa: E402

CLIENT = live.PeerRunsHTTPClient

WANTED = {
    "valid_code", "response_error_code", "run_path", "run_status", "next_poll_delay",
    "require_room_grant", "request", "admission_body", "invitation_body",
    "digest_check", "pre_admission", "http_error", "budget_error",
}


def bare(**attrs):
    """A client with no socket, no store and a frozen clock."""
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
    """PeerRunsHTTPError -> the flag object both sides compare."""
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
    """Materialise a fixture error spec as the live exception."""
    spec = spec or {}
    return live.PeerRunsHTTPError(
        spec.get("message", ""),
        retryable=bool(spec.get("retryable")),
        ambiguous=bool(spec.get("ambiguous")),
        not_admitted=bool(spec.get("not_admitted")),
        status_code=spec.get("status_code"),
        error_code=spec.get("error_code"),
    )


class Resp:
    """Minimal response carrier: declared length + read1 chunk source."""

    def __init__(self, raw, declared=None):
        self._body = io.BytesIO(raw.encode("utf-8"))
        self.headers = {} if declared is None else {"Content-Length": str(declared)}

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        return False

    def read1(self, size):
        return self._body.read1(size)


# --------------------------------------------------------------------------
# gates
# --------------------------------------------------------------------------

def f_valid_code(r):
    return live._valid_code(r.get("code"))


def f_response_error_code(r):
    return live._response_error_code(r.get("detail"))


def f_run_path(r):
    return live._run_path({"run_id": r.get("run_id")}, *(r.get("suffix") or []))


def f_run_status(r):
    return live._run_status(r.get("full") or {})


def f_next_poll_delay(r):
    c = bare(poll_min_seconds=r.get("poll_min_seconds", 0.1),
             poll_max_seconds=r.get("poll_max_seconds", 2.0))
    return c._next_poll_delay(r.get("cached"))


def f_require_room_grant(r):
    try:
        return {"ok": True, "value": CLIENT._require_room_grant(r.get("grant"))}
    except live.PeerRunsHTTPError as exc:
        return {"ok": False, "error": project(exc)}


def f_request(r):
    captured = {}
    response = r.get("response") or {}
    kind = response.get("kind", "ok")

    def fake_open_credentialed_url(request, timeout=None):
        captured["request"] = request
        if kind == "http":
            raise urllib.error.HTTPError(
                request.full_url, int(response.get("status")), "peer error", None,
                io.BytesIO(str(response.get("detail") or "").encode("utf-8")))
        if kind == "unreachable":
            if response.get("gaierror"):
                raise urllib.error.URLError(socket.gaierror(11001, "getaddrinfo failed"))
            name = response.get("errno_name")
            code = getattr(errno, name, 0) if name else 0
            raise urllib.error.URLError(OSError(code, "os error"))
        return Resp(response.get("raw") or "", response.get("declared"))

    original = url_security.open_credentialed_url
    url_security.open_credentialed_url = fake_open_credentialed_url
    try:
        c = bare(base_url=r["base_url"], api_key=r.get("api_key", ""))
        try:
            payload = c._request(r.get("path", ""), method=r.get("method", "GET"),
                                 body=r.get("body"), headers=r.get("headers"),
                                 room_grant=r.get("room_grant"))
        except live.PeerRunsHTTPError as exc:
            out = {"ok": False, "error": project(exc)}
        except (urllib.error.URLError, TimeoutError, OSError) as exc:  # pragma: no cover
            raise AssertionError("opener error escaped: %r" % (exc,))
        else:
            out = {"ok": True, "value": payload}
    finally:
        url_security.open_credentialed_url = original

    request = captured.get("request")
    assert request is not None, "the opener seam was never reached"
    grant = r.get("room_grant")
    merged = {
        "Authorization": ("HermesRoom " + str(grant)) if grant else
                         ("Bearer " + (r.get("api_key") or "")),
        "Content-Type": "application/json",
        "User-Agent": "Hermes-RoomLink/1.0",
        **(r.get("headers") or {}),
    }
    for name, value in merged.items():
        got = request.get_header(name.capitalize())
        assert got == value, "header %s observed %r, expected %r" % (name, got, value)
    out["request"] = {
        "url": request.full_url,
        "method": request.get_method(),
        "authorization": request.get_header("Authorization"),
        "content_type": request.get_header("Content-type"),
        "user_agent": request.get_header("User-agent"),
        "headers": merged,
        "data": request.data.decode("utf-8") if request.data is not None else None,
        "ambiguous": request.get_method() == "POST",
    }
    return out


def f_admission_body(r):
    dispatch = live.HostedMemberDispatch.from_mapping(r["dispatch"])
    return {
        "body": {"input": dispatch.prompt, "hosted_room_dispatch": dispatch.as_mapping()},
        "idempotency_key": "room:%s:%s" % (dispatch.task_id, dispatch.execution_generation),
    }


def f_invitation_body(r):
    captured = {}
    c = bare(api_key=r.get("api_key") or "")
    kwargs = {
        "room_id": r.get("room_id"),
        "home_install_id": r.get("home_install_id"),
        "authority_gateway_id": r.get("authority_gateway_id"),
        "authority_epoch": r.get("authority_epoch"),
        "member_id": r.get("member_id"),
        "grant_id": r.get("grant_id"),
        "ttl_seconds": r.get("ttl_seconds", 3600),
    }
    if "status_ttl_seconds" in r:
        kwargs["status_ttl_seconds"] = r.get("status_ttl_seconds")

    def fake_request(path, *, method="GET", body=None, headers=None, room_grant=None):
        captured["path"] = path
        captured["method"] = method
        captured["body"] = body
        return {"grant": "minted"}

    c._request = fake_request
    try:
        c.issue_invitation(**kwargs)
    except live.PeerRunsHTTPError as exc:
        return {"ok": False, "error": project(exc)}
    assert captured.get("path") == "/v1/room-members/invitations", captured
    return {"ok": True, "value": captured["body"]}


def f_digest_check(r):
    catalog = r.get("catalog") or {}
    policy = catalog.get("execution_policy") or {}
    carrier = SimpleNamespace(
        execution_policy=SimpleNamespace(policy_digest=policy.get("policy_digest")),
        catalog_digest=catalog.get("catalog_digest"),
    )
    error = live.digest_reauthorization_error(
        carrier,
        capability_digest=r.get("capability_digest"),
        execution_policy_digest=r.get("execution_policy_digest"),
    )
    return None if error is None else project(error)


def f_pre_admission(r):
    if r.get("gaierror"):
        exc = urllib.error.URLError(socket.gaierror(11001, "getaddrinfo failed"))
    elif r.get("oserror"):
        name = r.get("errno_name")
        exc = urllib.error.URLError(OSError(getattr(errno, name, 0) if name else 0, "os error"))
    else:
        exc = urllib.error.URLError("connection reset")
    ambiguous = bool(r.get("ambiguous"))
    not_admitted = ambiguous and live._is_proven_pre_admission_failure(exc)
    return project(live.PeerRunsHTTPError(
        "peer RoomLink endpoint is unreachable", retryable=True,
        ambiguous=ambiguous and not not_admitted, not_admitted=not_admitted))


def f_http_error(r):
    path = r.get("path", "")
    exc = urllib.error.HTTPError("https://peer.example" + path, int(r.get("status")),
                                 "peer error", None,
                                 io.BytesIO(str(r.get("detail") or "").encode("utf-8")))
    try:
        CLIENT._raise_http_error(exc, method=r.get("method", "GET"), path=path,
                                 deadline=time.monotonic() + 60.0)
    except live.PeerRunsHTTPError as err:
        return project(err)
    raise AssertionError("_raise_http_error returned instead of raising")


def f_budget_error(r):
    kind = r.get("kind", "")
    ambiguous = bool(r.get("ambiguous"))
    if r.get("budget", "size") == "time":
        resp = Resp("x" * 8)
        deadline = time.monotonic() - 1.0
    else:
        resp = Resp("", declared=live.MAX_PEER_RESPONSE_BYTES + 1)
        deadline = time.monotonic() + 60.0
    try:
        live._read_body(resp, max_bytes=live.MAX_PEER_RESPONSE_BYTES, deadline=deadline,
                        kind=kind, ambiguous=ambiguous)
    except live.PeerRunsHTTPError as exc:
        return {"ok": False, "error": project(exc)}
    raise AssertionError("_read_body did not enforce its budget")


DISPATCH = {
    "valid_code": f_valid_code,
    "response_error_code": f_response_error_code,
    "run_path": f_run_path,
    "run_status": f_run_status,
    "next_poll_delay": f_next_poll_delay,
    "require_room_grant": f_require_room_grant,
    "request": f_request,
    "admission_body": f_admission_body,
    "invitation_body": f_invitation_body,
    "digest_check": f_digest_check,
    "pre_admission": f_pre_admission,
    "http_error": f_http_error,
    "budget_error": f_budget_error,
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
