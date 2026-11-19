"""twin for gateway/hosted_rooms.py:_migrate_remote_run_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_rooms.py"
    PY_FN = "_migrate_remote_run_schema"
    C_FN = "_migrate_remote_run_schema"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
