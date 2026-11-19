"""twin for agent/anthropic_message_convert.py:_extract_preserved_thinking_blocks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_message_convert.py"
    PY_FN = "_extract_preserved_thinking_blocks"
    C_FN = "_extract_preserved_thinking_blocks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
