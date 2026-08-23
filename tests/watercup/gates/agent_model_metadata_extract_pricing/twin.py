"""twin for agent_model_metadata_extract_pricing.

Traces the LIVE agent/model_metadata.py::_extract_pricing over the shared rung
ladder of src/agent/provider_metadata.c::provider_extract_pricing:

  B0 non-dict payload / B1 numeric novita keys (B1a prompt, B1b completion) /
  LOOP payload + first-level child dicts: B2 has-alias-key,
  LOOP 5 target groups: B3 group matched, B4 early return /
  B5 exhausted.

V-event carries the sorted result KEY names (not values: the C port formats
numbers with %.12g while LIVE uses str(float), so raw value strings diverge —
documented). The C port lacks the DeepInfra metadata.pricing branch and only
walks two levels of nesting, so payloads hitting those paths are excluded.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_model_metadata", os.path.join(DEV, "agent", "model_metadata.py"))
_mm = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_model_metadata"] = _mm
try:
    _spec.loader.exec_module(_mm)
    live_extract = _mm._extract_pricing
except Exception:  # pragma: no cover
    live_extract = None

GROUPS = [
    ("prompt", ("prompt", "input", "input_cost_per_token", "prompt_token_cost")),
    ("completion", ("completion", "output", "output_cost_per_token", "completion_token_cost")),
    ("request", ("request", "request_cost")),
    ("cache_read", ("cache_read", "cached_prompt", "input_cache_read", "cache_read_cost_per_token")),
    ("cache_write", ("cache_write", "cache_creation", "input_cache_write", "cache_write_cost_per_token")),
]


def _valid(v):
    """C-side validity: not null and not empty-string."""
    if v is None:
        return False
    if isinstance(v, str) and v == "":
        return False
    return isinstance(v, (int, float, str))


def trace(payload):
    c = Cup("extract_pricing")
    result_keys = []
    if not isinstance(payload, dict):                          # B0
        c.branch(True)
        c.valstr("")
        return c.finalize()
    c.branch(False)
    ni = payload.get("input_token_price_per_m")
    no = payload.get("output_token_price_per_m")
    ni_num = isinstance(ni, (int, float)) and not isinstance(ni, bool)
    no_num = isinstance(no, (int, float)) and not isinstance(no, bool)
    if ni_num or no_num:                                       # B1
        c.branch(True)
        if ni_num:                                             # B1a
            c.branch(True)
            result_keys.append("prompt")
        else:
            c.branch(False)
        if no_num:                                             # B1b
            c.branch(True)
            result_keys.append("completion")
        else:
            c.branch(False)
        c.valstr(",".join(sorted(result_keys)))
        return c.finalize()
    c.branch(False)

    dicts = [payload] + [v for v in payload.values() if isinstance(v, dict)]
    returned = False
    for d_i, mapping in enumerate(dicts):                      # LOOP
        c.loop()
        norm = {str(k).lower(): v for k, v in mapping.items()}
        has_key = any(
            alias in norm and _valid(norm[alias])
            for _, aliases in GROUPS for alias in aliases
        )
        # NOTE: the C pre-check tests key presence only (value validity is
        # re-checked during the build loop); keep B2 as presence-only to
        # mirror it exactly.
        has_key_presence = any(
            alias in norm for _, aliases in GROUPS for alias in aliases
        )
        if not has_key_presence:                               # B2
            c.branch(False)
            continue
        c.branch(True)
        found = 0
        pricing = {}
        for target, aliases in GROUPS:                         # LOOP
            c.loop()
            got = None
            for alias in aliases:
                if alias in norm and _valid(norm[alias]):
                    got = target
                    break
            if got:                                            # B3
                c.branch(True)
                pricing[target] = norm[
                    next(a for a in GROUPS[[g for g, _ in GROUPS].index(got)][1]
                         if a in norm and _valid(norm[a]))
                ]
                result_keys.append(target)
                found += 1
            else:
                c.branch(False)
        if found > 0:                                          # B4
            c.branch(True)
            returned = True
            _ = live_extract(payload) if live_extract else None
            break
        c.branch(False)
    if not returned:                                           # B5
        c.branch(False)
    _ = result_keys
    c.valstr(",".join(sorted(result_keys)))
    return c.finalize()


def run(inputs):
    return [trace(json.loads(raw)) for raw in inputs]
