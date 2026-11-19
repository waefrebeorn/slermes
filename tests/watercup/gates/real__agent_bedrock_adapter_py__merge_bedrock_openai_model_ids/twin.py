"""twin for agent/bedrock_adapter.py:merge_bedrock_openai_model_ids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "merge_bedrock_openai_model_ids"
    C_FN = "merge_bedrock_openai_model_ids"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
