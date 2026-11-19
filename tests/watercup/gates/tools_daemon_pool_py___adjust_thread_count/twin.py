"""twin for tools/daemon_pool.py:_adjust_thread_count (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/daemon_pool.py"
    PY_FN = "_adjust_thread_count"
    C_FN = "tools_daemon_pool_u_adjust_thread_count"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
