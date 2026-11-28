"""twin for agent/bedrock_adapter.py:bedrock_static_context_length (table path).
None -> -1 (the C sentinel). Fixtures hit the table so no family fallback."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "bedrock_static_context_length"
    C_FN = "agent_bedrock_adapter_u_static_bedrock_context_length"
    def norm(self, c, result):
        c.valstr(str(-1 if result is None else int(result)))
_t = _Twin(); trace = _t.trace; run = _t.run
