"""twin for tools/flux3_video_tool.py:_poll_is_finished (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/flux3_video_tool.py"
    PY_FN = "_poll_is_finished"
    C_FN = "f3_poll_is_finished"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
