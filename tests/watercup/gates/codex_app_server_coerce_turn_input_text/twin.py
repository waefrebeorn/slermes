"""twin for codex_app_server_coerce_turn_input_text.

Mirrors the LIVE agent/transports/codex_app_server_session.py::
_coerce_turn_input_text body verbatim with Cup instrumentation, tracing the
SAME ladder as the C harness. Inputs are JSON-encoded values.
"""
import os, sys, json as _json

from watercup_py import Cup

DEFAULT_QUESTION = "What do you see in this image?"


def trace(user_input):
    c = Cup("coerce_turn_input_text")
    if isinstance(user_input, str):                          # B1
        c.branch(True)
        c.valstr(user_input)
        return c.finalize()
    c.branch(False)
    if isinstance(user_input, list):                         # B2
        c.branch(True)
        parts = []
        for item in user_input:
            c.loop()
            if isinstance(item, str):                        # B3
                c.branch(True); c.branch(False); c.branch(False); c.branch(False); c.branch(False)
                if not item.strip():                         # B4
                    c.branch(True)
                    continue
                c.branch(False)
                parts.append(item)
            elif isinstance(item, dict):                     # B5
                c.branch(False); c.branch(True); c.branch(False); c.branch(False); c.branch(False)
                t = item.get("type")
                is_text = t in ("text", "input_text")
                is_img = t in ("image", "image_url", "input_image")
                if is_text:                                  # B6
                    c.branch(True); c.branch(False)
                    text = item.get("text") or item.get("content") or ""
                    if text:                                 # B7
                        c.branch(True)
                        parts.append(str(text))
                    else:
                        c.branch(False)
                elif is_img:                                 # B8
                    c.branch(False); c.branch(True)
                    parts.append("[image attached]")
                else:
                    c.branch(False); c.branch(False)
            elif item is None:                               # B9
                c.branch(False); c.branch(False); c.branch(True); c.branch(False); c.branch(False)
                continue
            else:                                            # B10 scalar
                c.branch(False); c.branch(False); c.branch(False); c.branch(True); c.branch(False)
                if isinstance(item, bool):
                    parts.append("True" if item else "False")
                elif isinstance(item, int):
                    parts.append(str(item))
        text = "\n\n".join(p for p in parts).strip()
        if not text:                                         # B11
            c.branch(True)
            text = DEFAULT_QUESTION
        else:
            c.branch(False)
        c.valstr(text)
        return c.finalize()
    c.branch(False)
    if user_input is None:                                   # B12
        c.branch(True); c.branch(False)
        c.valstr("")
        return c.finalize()
    c.branch(False); c.branch(True)                          # B13
    if isinstance(user_input, bool):
        c.valstr("True" if user_input else "False")
    elif isinstance(user_input, int):
        c.valstr(str(user_input))
    else:
        c.valstr("")
    return c.finalize()


def run(inputs):
    return [trace(_json.loads(raw)) for raw in inputs]
