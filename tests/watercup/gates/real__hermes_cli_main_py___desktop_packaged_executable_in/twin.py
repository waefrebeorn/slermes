"""twin for hermes_cli/main.py:_desktop_packaged_executable_in (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_desktop_packaged_executable_in"
    C_FN = "_desktop_packaged_executable_in"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
