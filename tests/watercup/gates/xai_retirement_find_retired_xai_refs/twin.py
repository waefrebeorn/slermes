"""twin for xai_retirement_find_retired_xai_refs.

Imports the LIVE Python hermes_cli/xai_retirement.py::find_retired_xai_refs
and traces the SAME branch ladder as the C harness (src/xai_retirement.c
check_slot walk). Replacement/effort values come from LIVE _RETIRED_MODELS.

Input: "principal~delegation~vision~curator" ('~'-separated model slots;
empty field = slot unset).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_xai_retirement", os.path.join(DEV, "hermes_cli", "xai_retirement.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_xai_retirement"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

# Same slot order + retired-table order/content as src/xai_retirement.c.
SLOT_PATHS = ["principal.model", "delegation.model",
              "auxiliary.vision.model", "auxiliary.curator.model"]
RET_NAMES = ["grok-4", "grok-4-0709", "grok-4-fast", "grok-4-fast-reasoning",
             "grok-4-fast-non-reasoning", "grok-4-1-fast",
             "grok-4-1-fast-reasoning", "grok-4-1-fast-non-reasoning",
             "grok-code-fast-1", "grok-imagine-image-pro"]


def run(inputs):
    cups = []
    for raw in inputs:
        fields = (raw.split("~") + ["", "", "", ""])[:4]
        c = Cup("find_retired_xai_refs")
        issues = []

        for s, mv in enumerate(fields):
            norm = mod._normalize(mv) if mv and mv.strip() else ""
            looks = bool(norm.startswith("grok-"))
            if not looks:                                    # B1 looks-like-xAI
                c.branch(False)
                continue
            c.branch(True)
            for name in RET_NAMES:                           # L per comparison
                c.loop()
                if norm != name:                             # B2 match
                    c.branch(False)
                    continue
                c.branch(True)
                entry = mod._RETIRED_MODELS[norm]
                issues.append("%s|%s|%s|%s|" % (
                    SLOT_PATHS[s], mv,
                    entry["replacement"],
                    entry["reasoning_effort"] or ""))
                break

        c.valstr("".join(issues))
        cups.append(c.finalize())
    return cups
