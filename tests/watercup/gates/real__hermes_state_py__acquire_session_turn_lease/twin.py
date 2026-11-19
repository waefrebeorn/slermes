"""twin for hermes_state.py:acquire_session_turn_lease (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "acquire_session_turn_lease"
    C_FN = "acquire_session_turn_lease"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
