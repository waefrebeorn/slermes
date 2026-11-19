"""twin for electron/main.cjs:createDir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "electron/main.cjs"
    PY_FN = "createDir"
    C_FN = "dir_create"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
