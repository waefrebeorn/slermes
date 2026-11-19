"""twin for tools/skill_linter.py:_strip_code_blocks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_linter.py"
    PY_FN = "_strip_code_blocks"
    C_FN = "_strip_code_blocks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
