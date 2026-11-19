"""twin for agent/bedrock_adapter.py:configure_bedrock_openai_client_kwargs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "configure_bedrock_openai_client_kwargs"
    C_FN = "configure_bedrock_openai_client_kwargs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
