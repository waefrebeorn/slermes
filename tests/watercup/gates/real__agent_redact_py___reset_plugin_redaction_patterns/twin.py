"""twin for agent/redact.py:_reset_plugin_redaction_patterns (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/redact.py"
    PY_FN = "_reset_plugin_redaction_patterns"
    C_FN = "_reset_plugin_redaction_patterns"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
