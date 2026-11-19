"""twin for agent/conversation_loop.py:_image_error_max_dimension (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_image_error_max_dimension"
    C_FN = "cvl_image_error_max_dimension"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
