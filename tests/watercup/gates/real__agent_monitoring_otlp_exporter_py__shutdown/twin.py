"""twin for agent/monitoring/otlp_exporter.py:shutdown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/otlp_exporter.py"
    PY_FN = "shutdown"
    C_FN = "shutdown"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
