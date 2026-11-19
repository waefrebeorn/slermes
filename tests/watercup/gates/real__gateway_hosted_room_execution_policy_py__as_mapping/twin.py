"""twin for gateway/hosted_room_execution_policy.py:as_mapping (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_execution_policy.py"
    PY_FN = "as_mapping"
    C_FN = "as_mapping"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
