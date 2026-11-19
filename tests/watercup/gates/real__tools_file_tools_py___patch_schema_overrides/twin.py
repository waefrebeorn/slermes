"""twin for tools/file_tools.py:_patch_schema_overrides (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "_patch_schema_overrides"
    C_FN = "_patch_schema_overrides"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
