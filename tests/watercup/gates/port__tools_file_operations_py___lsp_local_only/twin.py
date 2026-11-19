"""twin for tools/file_operations.py:_lsp_local_only (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_operations.py"
    PY_FN = "_lsp_local_only"
    C_FN = "fo_lsp_local_only"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
