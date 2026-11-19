"""twin for tools/skill_manager_tool.py:_validate_content_size (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_manager_tool.py"
    PY_FN = "_validate_content_size"
    C_FN = "_validate_content_size"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
