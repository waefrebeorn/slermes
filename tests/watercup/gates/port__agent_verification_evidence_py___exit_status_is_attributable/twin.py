"""twin for agent/verification_evidence.py:_exit_status_is_attributable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verification_evidence.py"
    PY_FN = "_exit_status_is_attributable"
    C_FN = "_exit_status_is_attributable"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
