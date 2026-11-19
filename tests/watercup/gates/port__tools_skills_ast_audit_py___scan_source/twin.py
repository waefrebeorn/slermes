"""twin for tools/skills_ast_audit.py:_scan_source (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_ast_audit.py"
    PY_FN = "_scan_source"
    C_FN = "_scan_source"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
