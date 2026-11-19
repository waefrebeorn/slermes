"""twin for agent/anthropic_message_convert.py:convert_messages_to_anthropic (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_message_convert.py"
    PY_FN = "convert_messages_to_anthropic"
    C_FN = "convert_messages_to_anthropic"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
