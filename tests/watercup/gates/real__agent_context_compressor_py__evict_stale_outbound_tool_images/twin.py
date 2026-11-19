"""twin for agent/context_compressor.py:evict_stale_outbound_tool_images (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "evict_stale_outbound_tool_images"
    C_FN = "evict_stale_outbound_tool_images"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
