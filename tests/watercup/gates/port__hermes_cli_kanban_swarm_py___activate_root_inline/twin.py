"""twin for hermes_cli/kanban_swarm.py:_activate_root_inline (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_swarm.py"
    PY_FN = "_activate_root_inline"
    C_FN = "_activate_root_inline"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
