"""twin for tools/async_delegation.py:_children_activity_from_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/async_delegation.py"
    PY_FN = "_children_activity_from_token"
    C_FN = "_children_activity_from_token"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
