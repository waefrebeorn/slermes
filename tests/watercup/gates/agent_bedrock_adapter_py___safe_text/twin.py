"""twin for agent/bedrock_adapter.py:_safe_text (non-whitespace text else "(empty)")."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "_safe_text"
    C_FN = "agent_bedrock_adapter_u_safe_text"
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
