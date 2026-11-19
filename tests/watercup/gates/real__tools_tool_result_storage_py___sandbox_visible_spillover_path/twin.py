"""twin for tools/tool_result_storage.py:_sandbox_visible_spillover_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_result_storage.py"
    PY_FN = "_sandbox_visible_spillover_path"
    C_FN = "_sandbox_visible_spillover_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
