"""twin for tools/computer_use/backend.py:list_apps (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/backend.py"
    PY_FN = "list_apps"
    C_FN = "list_apps"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
