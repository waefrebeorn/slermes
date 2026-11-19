"""twin for gateway/hosted_room_policy_checkpoint.py:snapshot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_policy_checkpoint.py"
    PY_FN = "snapshot"
    C_FN = "snapshot"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
