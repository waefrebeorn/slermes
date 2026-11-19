"""twin for gateway/relay/adapter.py:_dm_top_level_threads_as_sessions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "_dm_top_level_threads_as_sessions"
    C_FN = "relay_helper_dm_top_level_threads_as_sessions"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
