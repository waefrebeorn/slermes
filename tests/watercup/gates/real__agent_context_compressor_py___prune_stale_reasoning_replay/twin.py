"""twin for agent/context_compressor.py:_prune_stale_reasoning_replay (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_prune_stale_reasoning_replay"
    C_FN = "_prune_stale_reasoning_replay"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
