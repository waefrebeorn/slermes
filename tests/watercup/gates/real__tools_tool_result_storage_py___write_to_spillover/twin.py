"""twin for tools/tool_result_storage.py:_write_to_spillover (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_result_storage.py"
    PY_FN = "_write_to_spillover"
    C_FN = "_write_to_spillover"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
