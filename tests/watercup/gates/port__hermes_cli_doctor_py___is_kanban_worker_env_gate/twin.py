"""twin for hermes_cli/doctor.py:_is_kanban_worker_env_gate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor.py"
    PY_FN = "_is_kanban_worker_env_gate"
    C_FN = "doctor_is_kanban_worker_env_gate"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
