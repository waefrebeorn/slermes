"""twin for tools/tool_result_storage.py:generate_preview (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_result_storage.py"
    PY_FN = "generate_preview"
    C_FN = "generate_preview"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
