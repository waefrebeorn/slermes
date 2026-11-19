"""twin for agent/monitoring/redaction.py:_secret_redact (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/redaction.py"
    PY_FN = "_secret_redact"
    C_FN = "agent_monitoring_redaction_u_secret_redact"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
