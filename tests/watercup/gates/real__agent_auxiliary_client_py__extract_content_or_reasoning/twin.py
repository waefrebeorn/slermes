"""twin for agent/auxiliary_client.py:extract_content_or_reasoning (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "extract_content_or_reasoning"
    C_FN = "extract_content_or_reasoning"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
