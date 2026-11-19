"""twin for tools/browser_use_cli.py:_floor_subprocess_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_use_cli.py"
    PY_FN = "_floor_subprocess_path"
    C_FN = "_floor_subprocess_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
