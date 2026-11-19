"""twin for agent/conversation_loop.py:_is_copilot_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_is_copilot_provider"
    C_FN = "cvl_is_copilot_provider"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
