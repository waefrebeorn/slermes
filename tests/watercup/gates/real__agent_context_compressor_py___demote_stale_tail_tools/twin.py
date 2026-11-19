"""twin for agent/context_compressor.py:_demote_stale_tail_tools (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_demote_stale_tail_tools"
    C_FN = "_demote_stale_tail_tools"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
