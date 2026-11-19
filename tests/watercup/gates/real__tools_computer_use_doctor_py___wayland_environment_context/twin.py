"""twin for tools/computer_use/doctor.py:_wayland_environment_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/doctor.py"
    PY_FN = "_wayland_environment_context"
    C_FN = "_wayland_environment_context"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
