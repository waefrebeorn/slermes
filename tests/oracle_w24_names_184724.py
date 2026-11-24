#!/usr/bin/env python3
"""Differential oracle for tools/connectors/gateway/names.py (gate w24_names_184724).

The live module is called directly -- it is a pure codec with no I/O, so every
comparison is against the real functions:

  * ``is_connector_name`` -- the prefix test over arbitrary objects;
  * ``parse_connector_name`` -- the bounded ``split("__", 2)`` into (connector, tool);
  * ``format_connector_name`` -- compose, stripping the ``<CONNECTOR>_`` prefix;
  * ``vendor_slug_candidates`` -- the ordered slug pair;
  * the module's ``__all__`` and the two literal constants.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_names_184724"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.connectors.gateway import names as live  # noqa: E402

WANTED = {"is_connector_name", "parse_connector_name", "format_connector_name",
          "vendor_slug_candidates", "constants"}


def _err(fn, *args):
    """Upstream raises AttributeError/TypeError for a non-str; name it."""
    try:
        return fn(*args)
    except (AttributeError, TypeError) as exc:
        return {"error": type(exc).__name__}


def expected(r):
    f = r.get("func")
    if f == "is_connector_name":
        return live.is_connector_name(r.get("name"))
    if f == "parse_connector_name":
        parsed = live.parse_connector_name(r.get("name"))
        return None if parsed is None else {"raw": parsed.raw, "connector": parsed.connector,
                                            "tool": parsed.tool}
    if f == "format_connector_name":
        return _err(live.format_connector_name, r.get("connector"), r.get("tool"))
    if f == "vendor_slug_candidates":
        out = _err(live.vendor_slug_candidates, r.get("connector"), r.get("tool"))
        return out if isinstance(out, dict) else list(out)
    if f == "constants":
        return {"CONNECTOR_NAME_PREFIX": live.CONNECTOR_NAME_PREFIX,
                "CONNECTOR_BATCH_SENTINEL": live.CONNECTOR_BATCH_SENTINEL,
                "__all__": list(live.__all__)}
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
