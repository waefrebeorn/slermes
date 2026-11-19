"""twin for gateway/hosted_room_driver.py:_task_schema_supports_current_statuses (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_driver.py"
    PY_FN = "_task_schema_supports_current_statuses"
    C_FN = "_task_schema_supports_current_statuses"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
