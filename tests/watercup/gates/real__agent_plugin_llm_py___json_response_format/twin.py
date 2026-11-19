"""twin for agent/plugin_llm.py:_json_response_format (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/plugin_llm.py"
    PY_FN = "_json_response_format"
    C_FN = "_json_response_format"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
