"""twin for agent/conversation_loop.py:_ensure_cached_system_prompt_static (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_ensure_cached_system_prompt_static"
    C_FN = "cvl_ensure_cached_system_prompt_static"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
