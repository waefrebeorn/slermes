"""twin for tools/clarify_tool.py:_batch_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/clarify_tool.py"
    PY_FN = "_batch_result"
    C_FN = "_batch_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
