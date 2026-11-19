"""twin for agent/bedrock_adapter.py:_model_supports_tool_use (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "_model_supports_tool_use"
    C_FN = "brd_model_supports_tool_use"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
