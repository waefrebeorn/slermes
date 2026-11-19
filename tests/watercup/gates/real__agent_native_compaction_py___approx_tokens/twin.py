"""twin for agent/native_compaction.py:_approx_tokens (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/native_compaction.py"
    PY_FN = "_approx_tokens"
    C_FN = "_approx_tokens"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
