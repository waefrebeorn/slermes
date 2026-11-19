"""twin for tools/file_state.py:_disabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_state.py"
    PY_FN = "_disabled"
    C_FN = "image_routing_vision_disabled"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
