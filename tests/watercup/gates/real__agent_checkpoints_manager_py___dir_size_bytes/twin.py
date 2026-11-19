"""twin for tools/checkpoint_manager.py:_dir_size_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/checkpoint_manager.py"
    PY_FN = "_dir_size_bytes"
    C_FN = "_dir_size_bytes"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
