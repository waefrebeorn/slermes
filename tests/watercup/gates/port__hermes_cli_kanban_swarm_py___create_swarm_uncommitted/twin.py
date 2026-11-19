"""twin for hermes_cli/kanban_swarm.py:_create_swarm_uncommitted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_swarm.py"
    PY_FN = "_create_swarm_uncommitted"
    C_FN = "_create_swarm_uncommitted"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
