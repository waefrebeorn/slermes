"""twin for agent/context_compressor.py:_load_proactive_prune_rearm_tokens (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_load_proactive_prune_rearm_tokens"
    C_FN = "cc_load_proactive_prune_rearm_tokens"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
