"""twin for hermes_cli/doctor.py:_doctor_web_capability_rows (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor.py"
    PY_FN = "_doctor_web_capability_rows"
    C_FN = "_doctor_web_capability_rows"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
