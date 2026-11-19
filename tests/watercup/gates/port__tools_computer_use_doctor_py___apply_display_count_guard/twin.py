"""twin for tools/computer_use/doctor.py:_apply_display_count_guard (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/doctor.py"
    PY_FN = "_apply_display_count_guard"
    C_FN = "_apply_display_count_guard"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
