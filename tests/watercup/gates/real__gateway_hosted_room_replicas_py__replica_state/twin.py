"""twin for gateway/hosted_room_replicas.py:replica_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_replicas.py"
    PY_FN = "replica_state"
    C_FN = "replica_state"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
