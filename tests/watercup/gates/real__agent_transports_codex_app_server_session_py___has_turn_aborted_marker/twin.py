"""twin for agent/transports/codex_app_server_session.py:_has_turn_aborted_marker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex_app_server_session.py"
    PY_FN = "_has_turn_aborted_marker"
    C_FN = "cas2_has_turn_aborted_marker"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
