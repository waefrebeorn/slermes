"""twin for tools/environments/local.py:cleanup_terminal_temp_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/local.py"
    PY_FN = "cleanup_terminal_temp_cache"
    C_FN = "cleanup_terminal_temp_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
