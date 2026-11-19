"""twin for hermes_state.py:_restore_journal_mode_after_repair (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_restore_journal_mode_after_repair"
    C_FN = "_restore_journal_mode_after_repair"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
