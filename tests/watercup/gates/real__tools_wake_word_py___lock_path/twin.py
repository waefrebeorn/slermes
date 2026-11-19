"""twin for tools/wake_word.py:_lock_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/wake_word.py"
    PY_FN = "_lock_path"
    C_FN = "_lock_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
