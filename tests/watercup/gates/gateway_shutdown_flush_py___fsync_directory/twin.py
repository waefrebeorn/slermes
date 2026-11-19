"""twin for gateway/shutdown_flush.py:_fsync_directory (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/shutdown_flush.py"
    PY_FN = "_fsync_directory"
    C_FN = "shutdown_flush_fsync_directory"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
