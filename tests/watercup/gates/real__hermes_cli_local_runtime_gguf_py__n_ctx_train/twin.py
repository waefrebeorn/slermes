"""twin for hermes_cli/local_runtime/gguf.py:n_ctx_train (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/gguf.py"
    PY_FN = "n_ctx_train"
    C_FN = "n_ctx_train"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
