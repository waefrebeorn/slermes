"""twin for hermes_cli/doctor.py:_honcho_is_configured_for_doctor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor.py"
    PY_FN = "_honcho_is_configured_for_doctor"
    C_FN = "doctor_honcho_is_configured"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
