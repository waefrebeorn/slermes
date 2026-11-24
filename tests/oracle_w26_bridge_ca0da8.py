#!/usr/bin/env python3
"""Differential oracle for tools/connectors/gateway/bridge.py (gate w26_bridge_ca0da8).

The live bridge is imported as a package module and driven with fakes for its
seams: the availability probe is a fixed bool, the gateway client is an object
whose ``search``/``schemas`` return (or raise) the fixture's fixed answer, and
no file is staged and nothing is executed.  What is compared is exactly what
those answers decide:

  * ``_leg_failure`` -- the D32 code a raised gateway error becomes;
  * ``connector_search_hits`` -- the search leg (silent on a dark gateway);
  * ``connector_describe`` -- the tools map, its schema resolution and the
    model-facing parameters (a file_uploadable input becomes a path string);
  * the literal-vendor-slug retry plan ``run_remote`` builds for confirmed
    TOOL_NOT_FOUND misses;
  * the module's fence literals.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_bridge_ca0da8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.connectors.gateway import bridge as live  # noqa: E402
from tools.connectors.gateway.names import vendor_slug_candidates  # noqa: E402

WANTED = {"leg_failure", "search_leg", "describe_leg", "fallback_plan", "constants"}


def make_exc(spec):
    """The exception object a seam raises, from the fixture's description."""
    if not isinstance(spec, dict):
        return None
    kind = spec.get("kind")
    code = spec.get("code", "GATEWAY_ERROR")
    status = spec.get("status")
    if kind == "auth":
        return live.GatewayAuthError("gateway said no", code=code, status=status)
    if kind == "unavailable":
        return live.GatewayUnavailable("gateway dark", code=code, status=status)
    return ValueError("unexpected")


def leg_failure(r):
    exc = make_exc({"kind": r.get("kind"), "code": r.get("code"), "status": r.get("status")})
    return live._leg_failure(exc)


def search_leg(r):
    result = r.get("result")
    exc = make_exc(r.get("raise"))

    class FakeClient:
        def search(self, queries):
            if exc is not None:
                raise exc
            return result

    leg = live.connector_search_hits(
        r.get("queries"),
        availability=lambda: r.get("available"),
        client_factory=lambda: FakeClient(),
    )
    return {"payload": leg.payload, "failure": leg.failure}


def describe_leg(r):
    response = r.get("schemas_response")
    exc = make_exc(r.get("raise"))

    class FakeClient:
        def schemas(self, slugs):
            if exc is not None:
                raise exc
            return response

    leg = live.connector_describe(
        r.get("names") or [],
        availability=lambda: r.get("available"),
        client_factory=lambda: FakeClient(),
    )
    return {"payload": leg.payload, "failure": leg.failure}


def fallback_plan(r):
    """The literal-retry selection run_remote performs over its entries."""
    planned = r.get("planned") or []
    entries = r.get("entries") or []
    out = []
    for slot, (plan, entry) in enumerate(zip(planned, entries)):
        primary, literal = vendor_slug_candidates(plan.get("connector"), plan.get("tool"))
        error = entry.get("error") if isinstance(entry, dict) else None
        if (
            primary != literal
            and isinstance(error, dict)
            and error.get("code") == "TOOL_NOT_FOUND"
        ):
            out.append({"slot": slot, "tool": literal})
    return out


def constants(_r):
    return {
        "SIGN_IN_EXPIRED": live.SIGN_IN_EXPIRED,
        "UNREACHABLE": live.UNREACHABLE,
        "NOT_SET_UP": live.NOT_SET_UP,
        "NOT_SET_UP_MESSAGE": live.NOT_SET_UP_MESSAGE,
    }


def expected(r):
    f = r.get("func")
    if f == "leg_failure":
        return leg_failure(r)
    if f == "search_leg":
        return search_leg(r)
    if f == "describe_leg":
        return describe_leg(r)
    if f == "fallback_plan":
        return fallback_plan(r)
    if f == "constants":
        return constants(r)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
