"""twin for agent/conversation_compression.py:resolve_compression_fallback_route (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "resolve_compression_fallback_route"
    C_FN = "resolve_compression_fallback_route"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
