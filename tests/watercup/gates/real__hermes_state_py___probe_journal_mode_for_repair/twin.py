"""twin for hermes_state.py:_probe_journal_mode_for_repair (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_probe_journal_mode_for_repair"
    C_FN = "_probe_journal_mode_for_repair"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
