"""twin for tools/browser_supervisor.py:_thread_main (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_supervisor.py"
    PY_FN = "_thread_main"
    C_FN = "_thread_main"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
