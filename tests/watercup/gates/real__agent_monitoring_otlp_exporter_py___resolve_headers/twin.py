"""twin for agent/monitoring/otlp_exporter.py:_resolve_headers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/otlp_exporter.py"
    PY_FN = "_resolve_headers"
    C_FN = "_resolve_headers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
