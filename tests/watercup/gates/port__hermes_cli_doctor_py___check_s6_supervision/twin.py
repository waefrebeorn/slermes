"""twin for hermes_cli/doctor.py:_check_s6_supervision (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor.py"
    PY_FN = "_check_s6_supervision"
    C_FN = "doctor_check_s6_supervision"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
