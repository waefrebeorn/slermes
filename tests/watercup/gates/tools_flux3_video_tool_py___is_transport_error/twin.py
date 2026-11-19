"""twin for tools/flux3_video_tool.py:_is_transport_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/flux3_video_tool.py"
    PY_FN = "_is_transport_error"
    C_FN = "f3_is_transport_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
