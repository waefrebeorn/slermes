"""twin for hermes_cli/local_runtime/binaries.py:runtimes_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/binaries.py"
    PY_FN = "runtimes_root"
    C_FN = "runtimes_root"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
