"""twin for agent/acp_openai_bridge.py:completion_to_stream_chunks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/acp_openai_bridge.py"
    PY_FN = "completion_to_stream_chunks"
    C_FN = "completion_to_stream_chunks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
