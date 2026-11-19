"""twin for hermes_cli/kanban.py:_check_dispatcher_presence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban.py"
    PY_FN = "_check_dispatcher_presence"
    C_FN = "_check_dispatcher_presence"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
