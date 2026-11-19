"""twin for agent/auxiliary_client.py:_translate_anthropic_response_format (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_translate_anthropic_response_format"
    C_FN = "_translate_anthropic_response_format"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
