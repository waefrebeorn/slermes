"""twin for agent/relay_llm.py:_preserve_pending_provider_chunks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_llm.py"
    PY_FN = "_preserve_pending_provider_chunks"
    C_FN = "_preserve_pending_provider_chunks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
