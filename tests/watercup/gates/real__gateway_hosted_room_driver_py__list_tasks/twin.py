"""twin for gateway/hosted_room_driver.py:list_tasks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_driver.py"
    PY_FN = "list_tasks"
    C_FN = "list_tasks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
