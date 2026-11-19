"""twin for hermes_cli/local_runtime/gguf.py:head_counts_kv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/gguf.py"
    PY_FN = "head_counts_kv"
    C_FN = "head_counts_kv"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
