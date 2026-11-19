"""twin for hermes_state.py:set_latest_matching_message_display_kind (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "set_latest_matching_message_display_kind"
    C_FN = "set_latest_matching_message_display_kind"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
