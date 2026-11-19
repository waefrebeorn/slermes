"""twin for agent/bedrock_adapter.py:_convert_content_to_converse (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "_convert_content_to_converse"
    C_FN = "_convert_content_to_converse"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
