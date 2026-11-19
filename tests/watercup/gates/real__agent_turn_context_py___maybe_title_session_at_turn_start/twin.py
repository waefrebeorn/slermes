"""twin for agent/turn_context.py:_maybe_title_session_at_turn_start (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_context.py"
    PY_FN = "_maybe_title_session_at_turn_start"
    C_FN = "_maybe_title_session_at_turn_start"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
