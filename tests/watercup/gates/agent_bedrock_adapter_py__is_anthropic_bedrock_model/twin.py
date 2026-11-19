"""twin for agent/bedrock_adapter.py:is_anthropic_bedrock_model (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "is_anthropic_bedrock_model"
    C_FN = "brd_is_anthropic_bedrock_model"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
