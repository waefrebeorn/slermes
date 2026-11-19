"""twin for agent/conversation_compression.py:try_shrink_image_parts_in_messages (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "try_shrink_image_parts_in_messages"
    C_FN = "try_shrink_image_parts_in_messages"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
