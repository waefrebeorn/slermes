"""twin for agent/monitoring/otlp_exporter.py:is_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/otlp_exporter.py"
    PY_FN = "is_enabled"
    C_FN = "otlp_exporter_is_enabled"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
