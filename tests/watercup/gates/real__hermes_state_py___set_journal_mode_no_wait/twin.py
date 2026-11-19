"""twin for hermes_state.py:_set_journal_mode_no_wait (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_set_journal_mode_no_wait"
    C_FN = "_set_journal_mode_no_wait"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
