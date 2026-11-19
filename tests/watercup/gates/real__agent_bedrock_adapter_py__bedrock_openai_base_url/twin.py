"""twin for agent/bedrock_adapter.py:bedrock_openai_base_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "bedrock_openai_base_url"
    C_FN = "bedrock_openai_base_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
