"""twin for hermes_state.py:_is_fts_write_corruption_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_is_fts_write_corruption_error"
    C_FN = "hermes_state_is_fts_write_corruption_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
