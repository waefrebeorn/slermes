"""twin for hermes_state.py:_is_duplicate_replayed_user_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_is_duplicate_replayed_user_message"
    C_FN = "_is_duplicate_replayed_user_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
