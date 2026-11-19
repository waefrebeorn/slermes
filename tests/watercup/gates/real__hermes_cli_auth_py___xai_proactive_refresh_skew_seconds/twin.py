"""twin for hermes_cli/auth.py:_xai_proactive_refresh_skew_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_xai_proactive_refresh_skew_seconds"
    C_FN = "auth_u_xai_proactive_refresh_skew_seconds"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
