"""twin for agent/bedrock_adapter.py:stream_converse_with_callbacks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "stream_converse_with_callbacks"
    C_FN = "stream_converse_with_callbacks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
