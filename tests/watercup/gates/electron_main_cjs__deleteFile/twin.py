"""twin for electron/main.cjs:deleteFile (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "electron/main.cjs"
    PY_FN = "deleteFile"
    C_FN = "file_delete"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
