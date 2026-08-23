"""twin for context_compressor_reasoning_details_text_chars.

LIVE agent/context_compressor.py::_reasoning_details_text_chars:

    if not value: return 0
    if isinstance(value, str): return len(value)
    total = 0
    if isinstance(value, dict): value = [value]
    if isinstance(value, list):
        for part in value:
            if isinstance(part, str): total += len(part)
            elif isinstance(part, dict):
                for text_key in ("thinking", "text", "summary"):
                    text = part.get(text_key)
                    if isinstance(text, str): total += len(text)
    return total

The twin traces the SAME rung order as the C harness (which scans the
'thinking' key only, plus its total==0 count-all-strings fallback), walking
the decoded JSON. Inputs stay inside the region where both agree: dicts carry
a string 'thinking' key, no text/summary-only parts.
"""
import json as _json
from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        v = _json.loads(raw) if raw else None
        # C parses "null" to a non-null JSON_NULL node, so only a missing
        # input counts as absent
        exists = raw != ""
        c = Cup("reasoning_details_text_chars")
        # rung A: absent value
        if not exists:
            c.branch(True)
            c.valstr("0")
            cups.append(c.finalize())
            continue
        c.branch(False)
        # rung B: plain string
        if isinstance(v, str):
            c.branch(True)
            c.valstr(str(len(v)))
            cups.append(c.finalize())
            continue
        c.branch(False)
        total = 0
        items = None
        obj = None
        # rung C/D: array vs object (C walks the object directly; equivalent
        # event shape: one branch per key)
        # C uses an if/else-if chain: only ONE branch event fires here
        if isinstance(v, list):
            c.branch(True)
            items = v
        elif isinstance(v, dict):
            c.branch(True)
            obj = v
        else:
            c.branch(False)
        if items is not None:
            for item in items:
                c.loop()
                if isinstance(item, str):
                    c.branch(True)
                    total += len(item)
                elif isinstance(item, dict):
                    c.branch(True)
                    for k in item.keys():
                        if k == "thinking" and isinstance(item[k], str):
                            c.branch(True)
                            total += len(item[k])
                        else:
                            c.branch(False)
                    if total == 0:
                        for k in item.keys():
                            if isinstance(item[k], str):
                                total += len(item[k])
                else:
                    c.branch(False)
        elif obj is not None:
            for k in obj.keys():
                if k == "thinking" and isinstance(obj[k], str):
                    c.branch(True)
                    total += len(obj[k])
                else:
                    c.branch(False)
            if total == 0:
                for k in obj.keys():
                    if isinstance(obj[k], str):
                        total += len(obj[k])
        c.valstr(str(total))
        cups.append(c.finalize())
    return cups
