"""twin for agent/conversation_loop.py:_restore_or_build_system_prompt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_restore_or_build_system_prompt"
    C_FN = "_restore_or_build_system_prompt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
