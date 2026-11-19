"""twin for agent/monitoring/otlp_exporter.py:build_exporter (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/otlp_exporter.py"
    PY_FN = "build_exporter"
    C_FN = "build_exporter"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
