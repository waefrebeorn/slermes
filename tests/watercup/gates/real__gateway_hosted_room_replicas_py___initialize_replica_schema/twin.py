"""twin for gateway/hosted_room_replicas.py:_initialize_replica_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_replicas.py"
    PY_FN = "_initialize_replica_schema"
    C_FN = "_initialize_replica_schema"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
