"""twin for hermes_state.py:_is_explicit_fork_child_row (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_is_explicit_fork_child_row"
    C_FN = "_is_explicit_fork_child_row"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
