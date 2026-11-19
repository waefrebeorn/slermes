"""twin for tools/kanban_tools.py:_is_dispatcher_owned_worker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/kanban_tools.py"
    PY_FN = "_is_dispatcher_owned_worker"
    C_FN = "_is_dispatcher_owned_worker"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
