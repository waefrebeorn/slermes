"""twin for tools/xai_http.py:hermes_xai_user_agent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/xai_http.py"
    PY_FN = "hermes_xai_user_agent"
    C_FN = "hermes_xai_user_agent"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
