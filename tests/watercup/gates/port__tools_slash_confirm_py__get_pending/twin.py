"""twin for tools/slash_confirm.py:get_pending (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/slash_confirm.py"
    PY_FN = "get_pending"
    C_FN = "get_pending"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
