"""twin for display_config_normalise.

Imports the LIVE gateway/display_config.py::_normalise and traces the SAME
branch ladder as the C harness (which calls the REAL C port for its result):

  B1 tool_progress / B2 bool-set / B3 grouping / B4 reasoning_style /
  B5 tool_preview_length / V normalised result string.

Inputs are "setting|value" pairs restricted to the agreed string-input
contract (see gate.json notes).
"""
import os
import sys
import importlib
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_display_config",
    os.path.join(DEV, "gateway", "display_config.py"))
_dc = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_display_config"] = _dc
_spec.loader.exec_module(_dc)

BOOL_SETTINGS = {
    "show_reasoning", "streaming", "interim_assistant_messages",
    "long_running_notifications", "busy_ack_detail", "cleanup_progress",
}


def run(inputs):
    cups = []
    for raw in inputs:
        setting, value = raw.split("|", 1)
        c = Cup("normalise")
        b1 = setting == "tool_progress"
        c.branch(b1)
        if not b1:
            is_bool = setting in BOOL_SETTINGS
            c.branch(is_bool)
            if not is_bool:
                c.branch(setting == "tool_progress_grouping")
                c.branch(setting == "reasoning_style")
                c.branch(setting == "tool_preview_length")
        result = _dc._normalise(setting, value)
        if isinstance(result, bool):
            result = "true" if result else "false"
        c.valstr(str(result))
        cups.append(c.finalize())
    return cups
