"""twin for hermes_cli/kanban_swarm.py:create_swarm (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_swarm.py"
    PY_FN = "create_swarm"
    C_FN = "create_swarm"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
