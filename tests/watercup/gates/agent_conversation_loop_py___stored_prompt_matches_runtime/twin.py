"""twin for agent/conversation_loop.py:_stored_prompt_matches_runtime (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_stored_prompt_matches_runtime"
    C_FN = "cvl_stored_prompt_matches_runtime"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
