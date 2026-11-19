"""twin for tools/file_tools.py:_invalidate_dedup_for_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "_invalidate_dedup_for_path"
    C_FN = "file_tools_invalidate_dedup_for_path"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
