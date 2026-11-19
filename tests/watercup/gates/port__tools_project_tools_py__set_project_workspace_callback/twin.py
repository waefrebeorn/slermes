"""twin for tools/project_tools.py:set_project_workspace_callback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/project_tools.py"
    PY_FN = "set_project_workspace_callback"
    C_FN = "tools_project_tools_set_project_workspace_callback"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
