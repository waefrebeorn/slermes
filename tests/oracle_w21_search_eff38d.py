#!/usr/bin/env python3
"""Differential oracle for tools/connectors/search.py (gate w21_search_eff38d).

The live module is imported and every ported function runs for real:
`connections_in_scope`, `connectors_unavailable`, `connector_entries_by_group`
and `remote_schemas_for` -- the latter two through their own injectable
`connector_search` / `connector_describe` parameters, so the gateway leg is the
fixture and no module global is rewritten. Entry construction runs the live
`format_connector_name` and `model_parameters` (schema transform).

Seams, named:
* `connector_search` / `connector_describe` -> a callable returning the
  fixture's `(failure, payload)` (the bridge call to the hosted gateway).
* `tools.connectors.gateway.config.guest_setup_failure()` -> the fixture's
  `last_failure` (the hosted-config probe).
* a catalog entry's `_tokens` are not compared: they are the Snowball-stemmed
  BM25 index (tools.tool_search_catalog._tokenize), not this module's decision.
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
SRC = QUARRY / "tools" / "connectors" / "search.py"
GATE = "w21_search_eff38d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("connectors_search_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["connectors_search_live"] = live
_spec.loader.exec_module(live)

import tools.connectors.gateway.config as gateway_config  # noqa: E402
from tools.connectors.gateway.files import model_parameters  # noqa: E402


class Leg:
    def __init__(self, failure, payload):
        self.failure = failure
        self.payload = payload


def canon_entry(entry):
    return {"name": entry.name, "description": entry.description, "schema": entry.schema,
            "source": entry.source, "source_name": entry.source_name}


def do_unavailable(r):
    gateway_config.guest_setup_failure = lambda: r.get("last_failure")
    return live.connectors_unavailable(r.get("failure"), verb=r.get("verb", ""),
                                       names=r.get("names"))


def do_entries(r):
    leg = r.get("leg") or {}
    payload = leg.get("payload")
    failure = leg.get("failure")

    def search(_requests):
        return Leg(failure, payload)

    queries = r.get("queries")
    if not isinstance(queries, list):
        queries = []
    per_query, leg_failure = live.connector_entries_by_group(queries, connector_search=search)
    return {"per_query": [[canon_entry(e) for e in group] for group in per_query],
            "failure": leg_failure}


def do_remote_schemas(r):
    leg = r.get("leg") or {}
    payload = leg.get("payload")
    failure = leg.get("failure")

    def describe(_names):
        return Leg(failure, payload)

    tools, leg_failure = live.remote_schemas_for(r.get("names") or [], r.get("tool_defs") or [],
                                                 connector_describe=describe)
    return {"tools": tools, "failure": leg_failure}


def expected(r):
    f = r.get("func")
    if f == "format_name":
        return live.format_connector_name(r.get("connector", ""), r.get("tool", ""))
    if f == "vendor_candidates":
        return list(live.vendor_slug_candidates(r.get("connector", ""), r.get("tool", "")))
    if f == "is_connector_name":
        return live.is_connector_name(r.get("name"))
    if f == "model_parameters":
        return model_parameters("probe", r.get("input_schema"))
    if f == "in_scope":
        return live.connections_in_scope(r.get("tool_defs") or [])
    if f == "unavailable":
        return do_unavailable(r)
    if f == "entries_by_group":
        return do_entries(r)
    if f == "remote_schemas":
        return do_remote_schemas(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"format_name", "vendor_candidates", "is_connector_name", "model_parameters",
              "in_scope", "unavailable", "entries_by_group", "remote_schemas"}
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
