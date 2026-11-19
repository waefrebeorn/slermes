"""twin for agent/conversation_compression.py:_emit_compression_attempt_telemetry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_emit_compression_attempt_telemetry"
    C_FN = "_emit_compression_attempt_telemetry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
