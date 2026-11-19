"""twin for agent/anthropic_message_convert.py:_content_parts_to_anthropic_blocks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_message_convert.py"
    PY_FN = "_content_parts_to_anthropic_blocks"
    C_FN = "_content_parts_to_anthropic_blocks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
