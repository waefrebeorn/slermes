#!/usr/bin/env python3
"""Differential oracle for tools/environments/sandbox_conformance.py part 1
(gate w33_sandboxcon_be5b73_p1).

The live module is imported and its value-only surface is called directly; no
provider, registry, tool or filesystem is involved:

  * ``hosted_scope_for`` - the Scope an agent is bound as;
  * ``_marker``          - the deterministic marker a check plants;
  * ``_identity``        - SandboxIdentity.key, the routing key;
  * ``checks()``         - the ordered check table with each callable's
    parameter names (inspect.signature);
  * the module literals STUB_BACKEND / PERSISTED_MARK / _METADATA_CALLS.
"""
import inspect
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_sandboxcon_be5b73_p1"

# tests/ must not be sys.path[0]: a tools package there would shadow the quarry.
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.environments import sandbox_conformance as live  # noqa: E402

WANTED = {"hosted_scope", "marker", "identity_key", "checks_table", "constants"}


def bare_scope(req):
    """The two attributes _marker/_identity read, with no Scope validation."""
    return types.SimpleNamespace(tenant=req.get("tenant", ""), agent=req.get("agent", ""))


def hosted_scope(req):
    kw = {}
    if "tenant" in req:
        kw["tenant"] = req["tenant"]
    if "user" in req:
        kw["user"] = req["user"]
    scope = live.hosted_scope_for(req.get("agent", ""), **kw)
    return {
        "tenant": scope.tenant,
        "agent": scope.agent,
        "actor_id": scope.actor.id,
        "owner_id": scope.owner.id,
        "can_administer": scope.can_administer,
    }


def marker(req):
    return live._marker(bare_scope(req))


def identity_key(req):
    return live._identity(bare_scope(req)).key


def checks_table(_req):
    rows = []
    for i, (name, fn) in enumerate(live.checks()):
        params = list(inspect.signature(fn).parameters)
        rows.append({"index": i, "name": name, "params": params, "arity": len(params)})
    return rows


def constants(_req):
    return {
        "stub_backend": live.STUB_BACKEND,
        "persisted_mark": live.PERSISTED_MARK,
        "metadata_calls": list(live._METADATA_CALLS),
    }


DISPATCH = {
    "hosted_scope": hosted_scope,
    "marker": marker,
    "identity_key": identity_key,
    "checks_table": checks_table,
    "constants": constants,
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if not cases:  # an empty stream proves nothing: that is a failed gate
        print("MISMATCH %s: no cases ran" % GATE, file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
