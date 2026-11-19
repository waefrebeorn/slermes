"""twin for hermes_cli/local_runtime/binaries.py:_download (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/binaries.py"
    PY_FN = "_download"
    C_FN = "_download"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
