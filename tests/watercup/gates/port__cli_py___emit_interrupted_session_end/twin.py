"""twin for cli.py:_emit_interrupted_session_end (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_emit_interrupted_session_end"
    C_FN = "_emit_interrupted_session_end"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
