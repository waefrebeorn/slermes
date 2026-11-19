"""twin for agent/monitoring/redaction.py:redact_for_export (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/redaction.py"
    PY_FN = "redact_for_export"
    C_FN = "agent_monitoring_redaction_redact_for_export"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
