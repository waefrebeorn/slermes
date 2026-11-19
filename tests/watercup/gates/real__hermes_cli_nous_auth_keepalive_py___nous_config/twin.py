"""twin for hermes_cli/nous_auth_keepalive.py:_nous_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_auth_keepalive.py"
    PY_FN = "_nous_config"
    C_FN = "_nous_config"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
