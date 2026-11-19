"""twin for hermes_cli/urllib_security.py:_resolved_https_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/urllib_security.py"
    PY_FN = "_resolved_https_context"
    C_FN = "hermes_cli_urllib_security_u_resolved_https_context"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
