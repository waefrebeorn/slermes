"""twin for goals_parse_contract.

Imports LIVE hermes_cli/goals.py::parse_contract and traces the same branch
ladder as goals_data.c::parse_contract. The V event is
headline \x1f outcome \x1f verification \x1f constraints \x1f boundaries \x1f stop_when.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_goals", os.path.join(DEV, "hermes_cli", "goals.py"))
mod = ilu.module_from_spec(spec); sys.modules["wc_goals"] = mod
spec.loader.exec_module(mod)
fn = mod.parse_contract
ALIASES = mod._CONTRACT_ALIASES

from watercup_py import Cup

def run(inputs):
    cups = []
    for text in inputs:
        try:
            headline, contract = fn(text)
        except Exception:
            headline, contract = "", mod.GoalContract()
        c = Cup("parse_contract")
        headline_parts = []
        if not text:
            c.branch(True)
        else:
            c.branch(False)
            for raw_line in text.splitlines():
                c.loop()
                line = raw_line.strip()
                blank = not line
                has_colon = (not blank) and (":" in line)
                matched = False
                if has_colon:
                    c.branch(True)
                    prefix, _, value = line.partition(":")
                    key = ALIASES.get(prefix.strip().lower())
                    matched = key is not None and bool(value.strip())
                    c.branch(matched)
                else:
                    c.branch(False)
                # mirror C: headline gets the line when unmatched AND non-blank
                keep = (not blank) and not matched
                c.branch(keep)
                if keep:
                    # mirror C's 'headline already has content' sub-rung
                    c.branch(bool(headline_parts))
                    headline_parts.append(line)
        fields = contract.to_dict() if hasattr(contract, "to_dict") else {}
        vals = [str(fields.get(f, "") or "") for f in ("outcome","verification","constraints","boundaries","stop_when")]
        c.valstr("\x1f".join([headline] + vals))
        cups.append(c.finalize())
    return cups
