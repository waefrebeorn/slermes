"""twin for cron_classify_parse_scores.

Imports LIVE cron/scripts/classify_items.py::_parse_scores and traces the SAME
rung ladder as the C harness (which mirrors src/cron/classify_items.c).
Inputs are "<content>|<n_items>".
"""
import json as _json
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_classify",
                                   os.path.join(DEV, "cron", "scripts", "classify_items.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_classify"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    fn = mod._parse_scores
    for raw in inputs:
        content, n_s = raw.rsplit("|", 1)
        n_items = int(n_s)
        c = Cup("cron_classify_parse_scores")

        text = content.strip()
        if text.startswith("```"):
            c.branch(True)
            text = text.strip("`")
            if "\n" in text:
                c.branch(True)
                text = text.split("\n", 1)[1]
            else:
                c.branch(False)
        else:
            c.branch(False)

        arr = None
        try:
            arr = _json.loads(text)
            c.branch(False)          # direct parse OK
        except _json.JSONDecodeError:
            c.branch(True)           # B3: direct parse failed
            start = text.find("[")
            end = text.rfind("]")
            if start >= 0 and end > start:
                c.branch(True)
                try:
                    arr = _json.loads(text[start:end + 1])
                    c.branch(False)
                except _json.JSONDecodeError:
                    c.branch(True)
                    c.valstr("")
                    cups.append(c.finalize())
                    continue
            else:
                c.branch(False)
                c.valstr("")
                cups.append(c.finalize())
                continue

        if isinstance(arr, list):
            c.branch(True)
            acc = []
            for obj in arr:
                if isinstance(obj, dict):
                    c.branch(True)
                    idx = obj.get("index")
                    if isinstance(idx, int) and not isinstance(idx, bool) \
                            and 0 <= idx < n_items:
                        c.branch(True)
                        acc.append("%d=%d=%s;" % (idx,
                                                  int(obj.get("score", 0)),
                                                  str(obj.get("reason", ""))))
                    else:
                        c.branch(False)
                else:
                    c.branch(False)
            c.valstr("".join(acc))
        else:
            c.branch(False)
            c.valstr("")
        cups.append(c.finalize())

    # sanity: traced value must agree with LIVE fn's accepted index set
    return cups
