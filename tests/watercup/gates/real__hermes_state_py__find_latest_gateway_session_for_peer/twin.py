"""twin for hermes_state.py:find_latest_gateway_session_for_peer (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "find_latest_gateway_session_for_peer"
    C_FN = "find_latest_gateway_session_for_peer"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
