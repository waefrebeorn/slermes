"""twin for gateway/media_repair.py:tool_name_by_call_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/media_repair.py"
    PY_FN = "tool_name_by_call_id"
    C_FN = "tool_name_by_call_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
