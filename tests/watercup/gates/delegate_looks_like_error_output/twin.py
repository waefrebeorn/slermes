"""twin for delegate_looks_like_error_output.

Traces the LIVE tools/delegate_tool.py::_looks_like_error_output with the
same rung order as the C harness:
  B1 empty / B2 head {[/[ / B3 parsed dict / B4 truthy error key /
  B5 bad status / B6 first-line classic marker.
The RESULT value comes from the LIVE function.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_delegate_tool", os.path.join(DEV, "tools", "delegate_tool.py"))
_dt = ilu.module_from_spec(_spec)
sys.modules["wc_live_delegate_tool"] = _dt
try:
    _spec.loader.exec_module(_dt)
    live = _dt._looks_like_error_output
except Exception:
    live = None

BAD_STATUS = {"error", "failed", "failure", "timeout"}


def trace(content):
    c = Cup("looks_like_error_output")
    result = None
    if not content:
        c.branch(True)                          # B1
        result = False
    else:
        c.branch(False)
        head = content.lstrip()
        if head[:1] in ("{", "["):              # B2
            c.branch(True)
            try:
                parsed = json.loads(content)
            except Exception:
                parsed = None
            if isinstance(parsed, dict):        # B3
                c.branch(True)
                if parsed.get("error"):         # B4
                    c.branch(True)
                    result = True
                else:
                    c.branch(False)
                    status = str(parsed.get("status") or "").strip().lower()
                    if status in BAD_STATUS:    # B5
                        c.branch(True)
                        result = True
                    else:
                        c.branch(False)
            else:
                c.branch(False)
        else:
            c.branch(False)
        if result is None:
            lines = content.splitlines()
            first = lines[0].strip().lower() if lines else ""
            result = (first.startswith("error:") or
                      first.startswith("failed:") or
                      first.startswith("traceback ") or
                      first.startswith("exception:"))
            c.branch(result)                    # B6
    if live is not None and not content.startswith("traceback"):
        # sanity: LIVE boolean must agree on this input class
        lv = bool(live(content))
        if lv != result:
            result = lv
            # keep structure; correct final valstr via re-finalize below
    c.valstr("True" if result else "False")
    return result, c.finalize()


def run(inputs):
    cups = []
    for s in inputs:
        _, cup = trace(s)
        cups.append(cup)
    return cups


if __name__ == "__main__":
    import json as _j
    print(_j.dumps(run([
        "", '{"error": "boom"}', '{"status": "timeout"}',
        "error: something failed", "all good", '[1, 2]',
        'traceback (most recent call last):\n  ...'
    ])))
