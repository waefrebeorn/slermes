"""twin for gateway/relay/adapter.py:_with_status_thread_anchor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "_with_status_thread_anchor"
    C_FN = "_with_status_thread_anchor"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
