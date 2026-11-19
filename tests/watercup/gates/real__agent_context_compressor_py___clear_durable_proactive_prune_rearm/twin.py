"""twin for agent/context_compressor.py:_clear_durable_proactive_prune_rearm (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_clear_durable_proactive_prune_rearm"
    C_FN = "cc_clear_durable_proactive_prune_rearm"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
