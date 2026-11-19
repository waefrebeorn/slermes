"""twin for tools/vision_tools.py:_materialize_video_from_terminal_backend (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/vision_tools.py"
    PY_FN = "_materialize_video_from_terminal_backend"
    C_FN = "_materialize_video_from_terminal_backend"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
