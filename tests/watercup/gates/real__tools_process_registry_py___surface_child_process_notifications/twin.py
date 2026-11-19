"""twin for tools/process_registry.py:_surface_child_process_notifications (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/process_registry.py"
    PY_FN = "_surface_child_process_notifications"
    C_FN = "_surface_child_process_notifications"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
