"""twin for gateway/slash_commands.py:_get_loop_manager_for_event (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_commands.py"
    PY_FN = "_get_loop_manager_for_event"
    C_FN = "_get_loop_manager_for_event"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
