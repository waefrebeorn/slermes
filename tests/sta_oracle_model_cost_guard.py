#!/usr/bin/env python3
"""Faithfulness oracle for port_model_cost_guard.c.

Reads a model-info JSON fixture from argv[1] and recomputes the SAME helpers
from the LIVE hermes_cli/model_cost_guard.py:
  _format_money(value)              -> "$X.YY/M" or "unknown"
  _pricing_from_model_info(mi)      -> (input, output, "models.dev") or (None,None,"")

The fixture JSON is a ModelInfo-shaped object with cost_input/cost_output
fields (or null / {} when cost data is absent). We build an agent.models_dev
.ModelInfo with those fields and call the real helper, then emit compact JSON
lines matching the C harness (runners diff them byte-for-byte).
"""
import sys, os, json

sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from hermes_cli.model_cost_guard import _format_money, _pricing_from_model_info
from agent.models_dev import ModelInfo


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_model_cost_guard.py <model_info.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        raw = f.read().strip()

    mi = None
    if raw and raw != "null":
        obj = json.loads(raw)
        if isinstance(obj, dict):
            mi = ModelInfo(
                id="",
                name=obj.get("name", ""),
                family=obj.get("family", ""),
                provider_id=obj.get("provider_id", ""),
                cost_input=float(obj.get("cost_input", 0) or 0),
                cost_output=float(obj.get("cost_output", 0) or 0),
            )

    in_cost, out_cost, source = _pricing_from_model_info(mi)
    present = in_cost is not None or out_cost is not None
    in_v = float(in_cost) if in_cost is not None else 0.0
    out_v = float(out_cost) if out_cost is not None else 0.0
    # libjson serializes whole-number floats WITHOUT a trailing ".0"
    # (e.g. 20.0 -> "20"), so mirror that to keep the oracle byte-exact.
    def jnum(v):
        iv = int(v)
        return iv if float(iv) == v else v

    print(json.dumps({"fn": "pricing", "present": int(present),
                      "in": jnum(in_v), "out": jnum(out_v), "source": source},
                     separators=(",", ":"), ensure_ascii=False))

    fm_in = _format_money(in_cost) if present else _format_money(None)
    fm_out = _format_money(out_cost) if present else _format_money(None)
    print(json.dumps({"fn": "money", "in": fm_in, "out": fm_out},
                     separators=(",", ":"), ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
