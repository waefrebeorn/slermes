"""twin for agent/bedrock_adapter.py:normalize_converse_stream_events (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "normalize_converse_stream_events"
    C_FN = "normalize_converse_stream_events"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
