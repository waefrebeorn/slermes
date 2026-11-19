"""twin for hermes_cli/security_audit.py:cmd_security_audit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/security_audit.py"
    PY_FN = "cmd_security_audit"
    C_FN = "hermes_cli_security_audit_cmd_security_audit"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
