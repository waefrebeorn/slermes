#!/usr/bin/env python3
"""Differential oracle for tools/connectors/portal/policy.py
(gate w24_policy_2a6f06).

The LIVE portal policy module is imported and its composition functions are
called on the real PolicyBody-shaped inputs: a member body and a change object
are materialised from the wire as attribute-only objects, which is all the
module touches (``body.mode``, ``body.connectors``, ``change.connector``,
``change.enabled``, ``change.disabled_tools``).  No transport, no portal URL,
no request: ``compose_*`` returns the dict a write would carry.

Exceptions are the C error strings, compared one-for-one:
  InvalidMemberPolicy -> "ERROR:policy:<message>"
  KeyError            -> "ERROR:keyerror"
  TypeError           -> "ERROR:type"
  AttributeError      -> "ERROR:attribute"
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_policy_2a6f06"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.connectors.portal import policy as live  # noqa: E402

WANTED = {"layer", "connector_write", "tools_write", "constants"}


class _Obj:
    """Attribute-only stand-in for a pydantic body/change: exactly the fields
    the descriptor names, so a missing field raises AttributeError like the
    model would."""

    def __init__(self, fields):
        for key, value in fields.items():
            setattr(self, key, value)


def as_obj(value):
    if value is None:
        return None
    if not isinstance(value, dict):
        return value          # non-model value: upstream raises AttributeError
    return _Obj(value)


def mapped(fn):
    try:
        return fn()
    except live.InvalidMemberPolicy as exc:
        return "ERROR:policy:" + str(exc)
    except KeyError:
        return "ERROR:keyerror"
    except TypeError:
        return "ERROR:type"
    except AttributeError:
        return "ERROR:attribute"


def layer(req):
    def run():
        key, values, allow = live._layer(as_obj(req.get("member")))
        return {"key": key, "values": values, "allow": allow}
    return mapped(run)


def connector_write(req):
    member = as_obj(req.get("member"))
    change = as_obj(req.get("change"))
    return mapped(lambda: live.compose_connector_write(member, change))


def tools_write(req):
    member = as_obj(req.get("member"))
    change = as_obj(req.get("change"))
    return mapped(lambda: live.compose_tools_write(member, change))


def constants():
    return {
        "layers": list(live._LAYERS.keys()),
        "slug_pattern": "^[a-z0-9][a-z0-9_-]*$",
        "max_tool_slug_chars": 256,
    }


def expected(r):
    f = r.get("func")
    if f == "layer":
        return layer(r)
    if f == "connector_write":
        return connector_write(r)
    if f == "tools_write":
        return tools_write(r)
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
