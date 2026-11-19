"""twin for tools/credential_files.py:_iter_syncable_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/credential_files.py"
    PY_FN = "_iter_syncable_files"
    C_FN = "_iter_syncable_files"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
