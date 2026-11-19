"""twin for agent/deadline.py:kill_process_tree (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/deadline.py"
    PY_FN = "kill_process_tree"
    C_FN = "kill_process_tree"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
