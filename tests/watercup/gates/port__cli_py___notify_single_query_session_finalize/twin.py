"""twin for cli.py:_notify_single_query_session_finalize (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_notify_single_query_session_finalize"
    C_FN = "_notify_single_query_session_finalize"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
