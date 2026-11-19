"""twin for tools/checkpoint_manager.py:_save_ledger (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/checkpoint_manager.py"
    PY_FN = "_save_ledger"
    C_FN = "_save_ledger"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
