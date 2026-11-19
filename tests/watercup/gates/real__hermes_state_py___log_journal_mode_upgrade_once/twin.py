"""twin for hermes_state.py:_log_journal_mode_upgrade_once (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_log_journal_mode_upgrade_once"
    C_FN = "_log_journal_mode_upgrade_once"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
