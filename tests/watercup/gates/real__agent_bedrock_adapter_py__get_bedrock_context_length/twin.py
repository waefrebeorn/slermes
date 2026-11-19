"""twin for agent/bedrock_adapter.py:get_bedrock_context_length (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "get_bedrock_context_length"
    C_FN = "brd_get_bedrock_context_length"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
