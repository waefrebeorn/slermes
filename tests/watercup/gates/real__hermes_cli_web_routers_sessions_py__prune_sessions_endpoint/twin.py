"""twin for hermes_cli/web_routers/sessions.py:prune_sessions_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/sessions.py"
    PY_FN = "prune_sessions_endpoint"
    C_FN = "prune_sessions_endpoint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
