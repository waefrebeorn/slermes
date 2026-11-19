"""twin for tools/checkpoint_manager.py:_take (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/checkpoint_manager.py"
    PY_FN = "_take"
    C_FN = "take"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
