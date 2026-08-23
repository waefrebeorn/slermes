"""twin for base_resolve_channel_prompt.

Traces the LIVE gateway/platforms/base.py::resolve_channel_prompt with the
same rung order as the C harness. Input encoding:
"<extra_json>|<channel_id>|<parent_id>".
"""
import os
import sys
import json as _json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_platform_base",
    os.path.join(DEV, "gateway", "platforms", "base.py"))
_base = ilu.module_from_spec(_spec)
sys.modules["wc_live_platform_base"] = _base
_spec.loader.exec_module(_base)

_fn = _base.resolve_channel_prompt


def trace(enc):
    extra_json, _, rest = enc.partition("|")
    channel_id, _, parent_id = rest.partition("|")
    try:
        config_extra = _json.loads(extra_json) if extra_json else None
    except Exception:
        config_extra = None

    c = Cup("resolve_channel_prompt")
    result = None
    if config_extra is None:                          # B1
        c.branch(True)
    else:
        c.branch(False)
        prompts = config_extra.get("channel_prompts") or {}
        if not isinstance(prompts, dict):             # B2
            c.branch(True)
        else:
            c.branch(False)
            for key in (channel_id, parent_id):
                blank_key = not key                   # B3 / B6
                c.branch(blank_key)
                if blank_key:
                    continue
                raw = prompts.get(key)
                have = raw is not None and isinstance(raw, str)
                # C only reads JSON_STRING prompts; non-string JSON values
                # are excluded from this gate's agreed sub-contract.
                c.branch(have)                        # B4 / B7
                if have:
                    prompt = raw.strip()
                    if prompt:
                        result = prompt
                        break
            c.branch(result is not None)              # B5
    c.valstr(result if result else "NONE")            # V
    cup = c.finalize()

    live = _fn(config_extra if config_extra is not None else {},
               channel_id, parent_id or None) \
        if config_extra is not None else None
    assert (live or "NONE") == (result or "NONE"), \
        "LIVE disagreement on %r: %r vs %r" % (enc, live, result)
    return cup


def run(inputs):
    return [trace(e) for e in inputs]
