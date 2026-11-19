"""twin for tools/approval.py:_get_single_query_approval_mode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "_get_single_query_approval_mode"
    C_FN = "_get_single_query_approval_mode"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
