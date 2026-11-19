"""twin for tools/delegate_tool.py:_batch_prefix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_batch_prefix"
    C_FN = "_batch_prefix"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
