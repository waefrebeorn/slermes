"""twin for agent/context_compressor.py:_warn_reclamation_no_op (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_warn_reclamation_no_op"
    C_FN = "_warn_reclamation_no_op"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
