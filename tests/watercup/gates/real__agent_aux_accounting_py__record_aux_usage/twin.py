"""twin for agent/aux_accounting.py:record_aux_usage (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/aux_accounting.py"
    PY_FN = "record_aux_usage"
    C_FN = "record_aux_usage"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
