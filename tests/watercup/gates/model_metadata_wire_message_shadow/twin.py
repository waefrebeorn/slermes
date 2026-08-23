"""twin for model_metadata_wire_message_shadow.

Traces LIVE agent/model_metadata.py::_wire_message_shadow with the same rung
order as the C harness (see harness.c header). Input restriction: no
"timestamp" key and multimodal dicts always carry a string text_summary.
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


def _load(name, relpath):
    spec = ilu.spec_from_file_location(name, os.path.join(DEV, relpath))
    mod = ilu.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


_mm = _load("wc_live_model_metadata", "agent/model_metadata.py")

_IMAGE_TYPES = {"image", "image_url", "input_image"}


def trace(raw):
    c = Cup("wire_message_shadow")
    try:
        msg = _json.loads(raw)
    except Exception:
        msg = None
    if not isinstance(msg, dict):                          # B1
        c.branch(True)
        c.valstr("raw-passthrough")
        return c.finalize()
    c.branch(False)
    sidecar = msg.get("api_content")
    wins = (isinstance(sidecar, str) and bool(sidecar)
            and msg.get("role") in ("user", "assistant"))
    for k, v in msg.items():
        c.loop()
        if k in ("_anthropic_content_blocks", "reasoning_details"):  # B2
            c.branch(True)
            c.valstr("skip")
            continue
        c.branch(False)
        if k == "api_content":                             # B3
            c.branch(True)
            if wins:                                       # B3a
                c.branch(True)
                c.valstr("api_content->content")
            else:
                c.branch(False)
                c.valstr("api_content-drop")
            continue
        c.branch(False)
        if k == "content":                                 # B4
            c.branch(True)
            if wins:                                       # B4a
                c.branch(True)
                c.valstr("content-skip")
                continue
            c.branch(False)
            is_list = isinstance(v, list)
            c.branch(is_list)                              # B4b
            if is_list:
                marks = []
                for part in v:
                    c.loop()
                    is_img = isinstance(part, dict) and part.get("type") in _IMAGE_TYPES
                    c.branch(is_img)
                    marks.append("STRIP" if is_img else "keep")
                c.valstr("content-array:%d:%s" % (len(v), ",".join(marks)))
            else:
                mmflag = isinstance(v, dict) and bool(v.get("_multimodal"))
                c.branch(mmflag)                           # B4c
                if mmflag:
                    hassum = isinstance(v.get("text_summary"), str)
                    c.branch(hassum)                       # B4d
                    if hassum:
                        c.valstr("content-summary")
                    else:
                        c.valstr("content-copy")
                else:
                    c.valstr("content-copy")
            continue
        c.branch(False)
        c.valstr("copy")
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
