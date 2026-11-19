"""twin for gateway/hosted_room_execution_policy.py:reset_room_execution_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_execution_policy.py"
    PY_FN = "reset_room_execution_policy"
    C_FN = "reset_room_execution_policy"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
