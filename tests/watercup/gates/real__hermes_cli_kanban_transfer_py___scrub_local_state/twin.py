"""twin for hermes_cli/kanban_transfer.py:_scrub_local_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_transfer.py"
    PY_FN = "_scrub_local_state"
    C_FN = "_scrub_local_state"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
