"""twin for hermes_cli/web_routers/sessions.py:get_session_latest_descendant (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/sessions.py"
    PY_FN = "get_session_latest_descendant"
    C_FN = "get_session_latest_descendant"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
