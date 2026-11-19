"""twin for hermes_cli/platform_actions.py:_audit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/platform_actions.py"
    PY_FN = "_audit"
    C_FN = "hermes_cli_security_audit_star_run_security_audit"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
