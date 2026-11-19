"""twin for hermes_cli/local_runtime/estimator.py:profile_from_gguf (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/estimator.py"
    PY_FN = "profile_from_gguf"
    C_FN = "profile_from_gguf"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
