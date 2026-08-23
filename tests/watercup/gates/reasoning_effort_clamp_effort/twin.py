"""twin for reasoning_effort_clamp_effort.

Traces the LIVE agent/reasoning_effort.py::clamp_effort with the SAME rung
order as the C harness (transcribed control structure, live semantics):

  B1 no requested/supported / per-item L,B2 on-ladder /
  B3 norm-empty-or-supported / B4 overrides / per-override L,B5 key match +
  inner L,B6 mapped supported / B7 off-ladder request / candidate scan
  L,B8 !=none / B9 no candidates / weaker scan L,B10 nearest-weaker /
  B11 weaker-exists / V resolved effort.

Input: JSON [effort, supported_list, overrides_obj].
"""
import os
import json
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec_path = os.path.join(DEV, "agent", "reasoning_effort.py")
import importlib.util as _ilu
_spec = _ilu.spec_from_file_location("wc_live_reasoning_effort", _spec_path)
_re = _ilu.module_from_spec(_spec)
sys.modules["wc_live_reasoning_effort"] = _re
_spec.loader.exec_module(_re)

LADDER = tuple(_re.EFFORT_LADDER)


def trace(effort, supported, overrides):
    c = Cup("clamp_effort")
    result = effort

    requested = str(effort or "").strip().lower()
    have_supported = bool(supported)
    c.branch(not requested or not have_supported)          # B1
    if requested and have_supported:
        supported_norm = []
        for level in supported:
            c.loop()
            lv = str(level).strip().lower()
            on_ladder = lv in LADDER
            c.branch(on_ladder)                            # B2
            if on_ladder:
                supported_norm.append(lv)

        in_norm = requested in supported_norm
        c.branch(not supported_norm or in_norm)            # B3
        if supported_norm and not in_norm:
            c.branch(bool(overrides))                      # B4
            if overrides:
                for key in overrides:
                    c.loop()
                    match = key == requested
                    c.branch(match)                        # B5
                    if match:
                        mapped = overrides[key]
                        for lvl in supported_norm:
                            c.loop()
                            hit = mapped is not None and lvl == mapped
                            c.branch(hit)                  # B6
                            if hit:
                                result = mapped
                                return c._done(result)
            c.branch(requested not in LADDER)              # B7
            if requested in LADDER:
                ridx = LADDER.index(requested)
                candidates = []
                for lvl in supported_norm:
                    c.loop()
                    not_none = lvl != "none"
                    c.branch(not_none)                     # B8
                    if not_none:
                        candidates.append(lvl)
                c.branch(not candidates)                   # B9
                if candidates:
                    best_below = None
                    best_idx = -1
                    min_idx = None
                    for lvl in candidates:
                        c.loop()
                        idx = LADDER.index(lvl)
                        if min_idx is None or idx < min_idx:
                            min_idx = idx
                        better = idx < ridx and idx > best_idx
                        c.branch(better)                   # B10
                        if better:
                            best_idx = idx
                            best_below = lvl
                    c.branch(best_below is not None)       # B11
                    if best_below is not None:
                        result = best_below
                    else:
                        result = min(candidates, key=LADDER.index)
    return c._done(result)


def _done(self, result):
    self.valstr("" if result is None else result)
    return self.finalize()


Cup._done = _done


def run(inputs):
    out = []
    for raw in inputs:
        arr = json.loads(raw)
        effort = arr[0] if len(arr) > 0 else None
        supported = arr[1] if len(arr) > 1 else None
        overrides = arr[2] if len(arr) > 2 else {}
        if not isinstance(overrides, dict):
            overrides = {}
        out.append(trace(effort, supported, overrides))
    return out


if __name__ == "__main__":
    # sanity vs LIVE fn
    cases = [
        ["high", ["low", "high"], {}],
        ["xhigh", ["low", "high", "max"], {}],
        ["xhigh", ["low", "high", "max"], {"xhigh": "max"}],
    ]
    for eff, sup, ovr in cases:
        assert trace(eff, sup, ovr) == _re.clamp_effort(eff, sup, ovr), (eff, sup, ovr)
    print("twin ok")
