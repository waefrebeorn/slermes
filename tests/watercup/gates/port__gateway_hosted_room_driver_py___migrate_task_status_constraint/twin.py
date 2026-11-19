"""twin for gateway/hosted_room_driver.py:_migrate_task_status_constraint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_driver.py"
    PY_FN = "_migrate_task_status_constraint"
    C_FN = "_migrate_task_status_constraint"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
