"""twin for agent/monitoring/gateway_health_export.py:_emit_snapshot_events (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health_export.py"
    PY_FN = "_emit_snapshot_events"
    C_FN = "he_emit_snapshot_events"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
