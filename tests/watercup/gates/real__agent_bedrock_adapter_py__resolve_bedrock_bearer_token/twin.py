"""twin for agent/bedrock_adapter.py:resolve_bedrock_bearer_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "resolve_bedrock_bearer_token"
    C_FN = "resolve_bedrock_bearer_token"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
