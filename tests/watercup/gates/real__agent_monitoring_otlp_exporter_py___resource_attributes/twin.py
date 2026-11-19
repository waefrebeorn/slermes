"""twin for agent/monitoring/otlp_exporter.py:_resource_attributes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/otlp_exporter.py"
    PY_FN = "_resource_attributes"
    C_FN = "_resource_attributes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
