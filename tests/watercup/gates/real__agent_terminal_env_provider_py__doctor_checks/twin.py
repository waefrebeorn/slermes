"""twin for agent/terminal_env_provider.py:doctor_checks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/terminal_env_provider.py"
    PY_FN = "doctor_checks"
    C_FN = "doctor_checks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
