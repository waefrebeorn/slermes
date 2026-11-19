"""twin for tools/file_tools.py:_sentinel_free_abs_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "_sentinel_free_abs_cwd"
    C_FN = "_sentinel_free_abs_cwd"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
