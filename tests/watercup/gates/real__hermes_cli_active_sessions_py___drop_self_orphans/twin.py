"""twin for hermes_cli/active_sessions.py:_drop_self_orphans (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/active_sessions.py"
    PY_FN = "_drop_self_orphans"
    C_FN = "_drop_self_orphans"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
