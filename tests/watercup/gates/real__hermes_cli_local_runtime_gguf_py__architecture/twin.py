"""twin for hermes_cli/local_runtime/gguf.py:architecture (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/gguf.py"
    PY_FN = "architecture"
    C_FN = "architecture"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
