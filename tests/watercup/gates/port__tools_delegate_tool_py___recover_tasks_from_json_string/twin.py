"""twin for tools/delegate_tool.py:_recover_tasks_from_json_string (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_recover_tasks_from_json_string"
    C_FN = "_recover_tasks_from_json_string"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
