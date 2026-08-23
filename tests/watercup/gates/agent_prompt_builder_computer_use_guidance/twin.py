"""twin for agent_prompt_builder_computer_use_guidance.

Traces the LIVE agent/prompt_builder.py::computer_use_guidance branch ladder
with the same rung order as the C harness:
  B1 is_macos / B2 is_windows / B3-B5 offscreen_line variant selection.
Value events hash the SHORT per-platform fields (os_name, save_combo,
example_app); the long prose body is excluded (see gate notes — the LIVE
wording has drifted from the C port's embedded text).
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_prompt_builder", os.path.join(DEV, "agent", "prompt_builder.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_prompt_builder"] = _pb
_spec.loader.exec_module(_pb)


def trace(platform):
    c = Cup("computer_use_guidance")
    is_macos = platform == "darwin"
    is_windows = platform == "win32"
    if is_macos:                                   # B1
        c.branch(True)
        os_name, save_combo = "macOS", "cmd+s"
    else:
        c.branch(False)
        if is_windows:                             # B2
            c.branch(True)
            os_name = "Windows"
        else:
            c.branch(False)
            os_name = "Linux"
        save_combo = "ctrl+s"
    if is_macos:                                   # B3
        c.branch(True)
    elif is_windows:                               # B4
        c.branch(False)
        c.branch(True)
    else:                                          # B5
        c.branch(False)
        c.branch(False)
    example_app = "Safari" if is_macos else ("Chrome" if is_windows else "Firefox")
    c.valstr(os_name)
    c.valstr(save_combo)
    c.valstr(example_app)
    return c.finalize()


def run(inputs):
    cups = []
    for p in inputs:
        live = _pb.computer_use_guidance(p if p else None)
        if not live:
            raise RuntimeError("LIVE computer_use_guidance empty for %r" % p)
        cups.append(trace(p))
    return cups
