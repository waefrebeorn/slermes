"""twin for hermes_state.py:reopen_orphaned_compression_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "reopen_orphaned_compression_session"
    C_FN = "reopen_orphaned_compression_session"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
