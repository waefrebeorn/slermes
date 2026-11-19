"""twin for agent/anthropic_message_convert.py:_apply_assistant_cache_control_to_last_cacheable_block (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_message_convert.py"
    PY_FN = "_apply_assistant_cache_control_to_last_cacheable_block"
    C_FN = "_apply_assistant_cache_control_to_last_cacheable_block"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
