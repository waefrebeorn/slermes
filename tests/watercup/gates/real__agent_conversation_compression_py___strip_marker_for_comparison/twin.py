"""twin for agent/conversation_compression.py:_strip_marker_for_comparison (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_strip_marker_for_comparison"
    C_FN = "_strip_marker_for_comparison"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
