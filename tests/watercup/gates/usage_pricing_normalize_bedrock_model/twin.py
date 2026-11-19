"""twin for agent/usage_pricing.py:_normalize_bedrock_model_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/usage_pricing.py"
    PY_FN = "_normalize_bedrock_model_name"
    C_FN = "_normalize_bedrock_model_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
