"""twin for agent/bedrock_adapter.py:is_bedrock_openai_base_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "is_bedrock_openai_base_url"
    C_FN = "is_bedrock_openai_base_url"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
