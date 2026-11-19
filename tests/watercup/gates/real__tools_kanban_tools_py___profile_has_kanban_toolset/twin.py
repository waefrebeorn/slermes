"""twin for tools/kanban_tools.py:_profile_has_kanban_toolset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/kanban_tools.py"
    PY_FN = "_profile_has_kanban_toolset"
    C_FN = "profile_has_kanban_toolset"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
