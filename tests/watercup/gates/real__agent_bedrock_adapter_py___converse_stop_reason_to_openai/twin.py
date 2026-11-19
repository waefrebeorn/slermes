"""twin for agent/bedrock_adapter.py:_converse_stop_reason_to_openai (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "_converse_stop_reason_to_openai"
    C_FN = "_converse_stop_reason_to_openai"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
