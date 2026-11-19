"""twin for hermes_cli/auth.py:_normalize_lmstudio_runtime_base_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_normalize_lmstudio_runtime_base_url"
    C_FN = "normalize_lmstudio_runtime_base_url"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
