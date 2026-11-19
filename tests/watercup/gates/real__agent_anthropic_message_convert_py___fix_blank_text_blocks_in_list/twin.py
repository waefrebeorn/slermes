"""twin for agent/anthropic_message_convert.py:_fix_blank_text_blocks_in_list (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_message_convert.py"
    PY_FN = "_fix_blank_text_blocks_in_list"
    C_FN = "_fix_blank_text_blocks_in_list"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
