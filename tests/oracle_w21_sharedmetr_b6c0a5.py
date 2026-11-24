#!/usr/bin/env python3
"""Differential oracle for hermes_cli/observability/shared_metrics_drift.py
(gate w21_sharedmetr_b6c0a5).

The live module is imported as part of its package and its two functions are
called directly, so the contract tables, the identifier normalizer and the
fallback search are the module's own. One data seam: the catalog-backed enum
members of `_COUNTER_DIMENSION_VALUES` are `_CatalogValues` (filled from the
installed plugin catalog at run time, thousands of provider/extension names). A
fixture's `catalog` replaces exactly those members with the snapshot a packaging
run packaged with -- the same object the C side receives -- so both sides see
one closed set per field. Fields the fixture leaves out resolve to the empty set.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w21_sharedmetr_b6c0a5"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("hermes_cli.observability.shared_metrics_drift")
contract = importlib.import_module("hermes_cli.observability.shared_metrics_contract")
_CatalogValues = contract._CatalogValues

# The catalog-seam members, as the contract defines them before any catalog load.
CATALOG_MEMBERS = [(metric, field, allowed)
                   for metric, fields in contract._COUNTER_DIMENSION_VALUES.items()
                   for field, allowed in fields.items()
                   if isinstance(allowed, _CatalogValues)]


def apply_catalog(catalog):
    """Point every catalog-backed member at the fixture's snapshot (or nothing)."""
    for metric, field, _original in CATALOG_MEMBERS:
        supplied = ((catalog or {}).get(metric) or {}).get(field) or []
        contract._COUNTER_DIMENSION_VALUES[metric][field] = frozenset(
            v for v in supplied if isinstance(v, str))


def compute_shape():
    """The contract's shape, from the members captured before any catalog was applied."""
    catalog = {(metric, field) for metric, field, _original in CATALOG_MEMBERS}
    values = contract._COUNTER_DIMENSION_VALUES
    static = [(metric, field, allowed) for metric, fields in values.items()
              for field, allowed in fields.items() if (metric, field) not in catalog]
    return {
        "metrics": len(contract._METRIC_FIELDS),
        "metric_field_slots": sum(len(fields) for fields in contract._METRIC_FIELDS.values()),
        "legacy_sets": sum(len(sets) for sets in contract._LEGACY_METRIC_FIELDS.values()),
        "static_enum_fields": len(static),
        "catalog_fields": len(CATALOG_MEMBERS),
        "ident_fields": sum(len(fields) for fields in contract._IDENTIFIER_FIELDS.values()),
        "enum_values": sum(len(allowed) for _m, _f, allowed in static),
    }


# Frozen at import: apply_catalog() mutates the dict below on every case.
SHAPE = compute_shape()


def contract_shape():
    return dict(SHAPE)


def expected(r):
    f = r.get("func")
    if f == "contract_shape":
        return contract_shape()
    if f == "metric_identifier":
        return contract._metric_identifier(r.get("value"), max_length=r.get("max_length", 0))
    if f == "shape_known":
        return live.counter_dimensions_shape_is_known(r.get("metric"), r.get("dimensions") or {})
    if f == "conform":
        apply_catalog(r.get("catalog"))
        try:
            return {"status": "ok", "value": live.conform_counter_dimensions(
                r.get("metric"), r.get("dimensions") or {})}
        except Exception as exc:  # noqa: BLE001 - surfaced exactly as the C side shapes it
            return {"status": "error", "type": type(exc).__name__, "message": str(exc)}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"contract_shape", "metric_identifier", "shape_known", "conform"}
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
