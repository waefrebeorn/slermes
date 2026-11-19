"""twin for tools/checkpoint_manager.py:clear_legacy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/checkpoint_manager.py"
    PY_FN = "clear_legacy"
    C_FN = "clear_legacy"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
