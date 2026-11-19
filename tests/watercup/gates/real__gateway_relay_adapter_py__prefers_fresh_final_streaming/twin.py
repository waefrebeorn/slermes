"""twin for gateway/relay/adapter.py:prefers_fresh_final_streaming (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "prefers_fresh_final_streaming"
    C_FN = "prefers_fresh_final_streaming"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
