"""twin for cli.py:_should_emit_cleanup_session_finalize (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_should_emit_cleanup_session_finalize"
    C_FN = "cli_should_emit_cleanup_session_finalize"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
