"""twin for tools/skills_tool.py:_plugin_skill_linked_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_tool.py"
    PY_FN = "_plugin_skill_linked_files"
    C_FN = "_plugin_skill_linked_files"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
