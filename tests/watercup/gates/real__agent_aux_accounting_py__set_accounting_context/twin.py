"""twin for agent/aux_accounting.py:set_accounting_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/aux_accounting.py"
    PY_FN = "set_accounting_context"
    C_FN = "aux_set_accounting_context"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
