"""twin for agent/chat_completion_helpers.py:_fallback_entry_unavailable_without_network (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_fallback_entry_unavailable_without_network"
    C_FN = "_fallback_entry_unavailable_without_network"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
