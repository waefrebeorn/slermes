"""twin for agent/system_prompt.py:_tui_embedded_pane_clarifier.

Input "hint|||desktop(0/1)". Sets HERMES_DESKTOP_TERMINAL per scenario (the
python fn gates on it), calls the real fn, cups the returned string -- same
value the C harness cups.
"""
import os
from watercup_twin_runtime import TwinBase


class _Twin(TwinBase):
    PY_MODULE = "agent/system_prompt.py"
    PY_FN = "_tui_embedded_pane_clarifier"
    C_FN = "agent_system_prompt_u_tui_embedded_pane_clarifier"

    def call(self, inp, inputs, idx=0):
        s = str(inp)
        if "|||" in s:
            hint, flag = s.split("|||", 1)
        else:
            hint, flag = s, "0"
        if flag.strip() == "1":
            os.environ["HERMES_DESKTOP_TERMINAL"] = "1"
        else:
            os.environ.pop("HERMES_DESKTOP_TERMINAL", None)
        return self._fn(hint)

    def norm(self, c, result):
        c.valstr("" if result is None else str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
