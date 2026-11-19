"""twin for tools/computer_use/cua_backend.py:launch_app (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "launch_app"
    C_FN = "cua_launch_app"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
