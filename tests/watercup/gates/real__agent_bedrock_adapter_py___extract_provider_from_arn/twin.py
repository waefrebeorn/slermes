"""twin for agent/bedrock_adapter.py:_extract_provider_from_arn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "_extract_provider_from_arn"
    C_FN = "_extract_provider_from_arn"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
