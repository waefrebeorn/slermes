"""twin for tools/kanban_tools.py:_maybe_auto_subscribe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/kanban_tools.py"
    PY_FN = "_maybe_auto_subscribe"
    C_FN = "kbt_maybe_auto_subscribe"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
