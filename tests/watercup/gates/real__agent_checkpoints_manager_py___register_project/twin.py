"""twin for tools/checkpoint_manager.py:_register_project (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/checkpoint_manager.py"
    PY_FN = "_register_project"
    C_FN = "_register_project"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
